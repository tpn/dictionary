/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Word.c

Abstract:

    This module implements functionality related to dictionary words.  Routines
    are provided to initialize and compare words.

--*/

#include "stdafx.h"

//
// Word initialization.
//

_Use_decl_annotations_
BOOLEAN
InitializeWord(
    PCBYTE Bytes,
    ULONG MinimumLength,
    ULONG MaximumLength,
    PLONG_STRING String,
    PCHARACTER_BITMAP Bitmap,
    PCHARACTER_HISTOGRAM Histogram,
    PULONG BitmapHashPointer,
    PULONG HistogramHashPointer
    )
{
    BYTE Byte;
    BYTE TrailingBytes;
    HASH Hash;
    ULONG Index;
    ULONG Length;
    ULONG BitmapHash;
    ULONG StringHash;
    ULONG HistogramHash;
    ULONG NumberOfDoubleWords;
    PLONG Bits;
    PULONG Counts;
    PULONG DoubleWords;

    //
    // Verify arguments.
    //

    if (!ARGUMENT_PRESENT(Bytes)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(String)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Bitmap)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Histogram)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(BitmapHashPointer)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(HistogramHashPointer)) {
        return FALSE;
    }

    if (MinimumLength == 0 || MaximumLength == 0 ||
        MinimumLength > MaximumLength) {
        return FALSE;
    }

    //
    // Verify the bitmap and histogram are aligned for AVX2 intrinsics.
    //

    if (!IsAligned32(Bitmap) || !IsAligned32(Histogram)) {
        return FALSE;
    }

    //
    // Clear the caller's pointers to hashes up-front.
    //

    *BitmapHashPointer = 0;
    *HistogramHashPointer = 0;

    //
    // Zero the bitmap and histogram.
    //

    ZeroStructPointer(Bitmap);
    ZeroStructPointer(Histogram);

    //
    // Initialize aliases.
    //

    Length = 0;
    Bits = (PLONG)&Bitmap->Bits;
    Counts = (PULONG)&Histogram->Counts;

    //
    // Iterate over each byte in the string, increment the corresponding count
    // for the histogram and set the corresponding bit in the bitmap.
    //

    for (Index = 0; Index < MaximumLength; Index++) {
        Byte = Bytes[Index];
        if (Byte == '\0') {
            Length = Index;
            break;
        }
        Counts[Byte]++;
        BitTestAndSet(Bits, Byte);
    }

    if (!Length) {

        //
        // No NULL was found; the string is too long.  Return error.
        //

        return FALSE;
    }

    if (Length < MinimumLength) {

        //
        // String is too short.
        //

        return FALSE;
    }

    //
    // Calculate the bitmap hash.
    //

    BitmapHash = Length;
    for (Index = 0; Index < ARRAYSIZE(Bitmap->Bits); Index++) {
        Hash.Index = Index;
        Hash.Value = Bitmap->Bits[Index];
        BitmapHash = _mm_crc32_u32(BitmapHash, Hash.AsULong);
    }

    //
    // Calculate the histogram hash after a quick sanity check that our number
    // of character bits is a multiple 4.
    //

    ASSERT(!(NUMBER_OF_CHARACTER_BITS % 4));

    HistogramHash = Length;
    for (Index = 0; Index < NUMBER_OF_CHARACTER_BITS; Index++) {
        Hash.Index = Index;
        Hash.Value = Counts[Index];
        HistogramHash = _mm_crc32_u32(HistogramHash, Hash.AsULong);
    }

    //
    // Calculate the string hash.
    //

    StringHash = Length;
    DoubleWords = (PULONG)Bytes;
    TrailingBytes = Length % 4;
    NumberOfDoubleWords = Length >> 2;

    if (NumberOfDoubleWords) {

        //
        // Process as many 4 byte chunks as we can.
        //

        for (Index = 0; Index < NumberOfDoubleWords; Index++) {
            StringHash = _mm_crc32_u32(StringHash, DoubleWords[Index]);
        }
    }

    if (TrailingBytes) {

        //
        // There are between 1 and 3 bytes remaining at the end of the string.
        // We can't use _mm_crc32_u32() here directly on the last ULONG as we
        // will include the bytes past the end of the string, which will be
        // random and will affect our hash value.  So, load we load the last
        // ULONG then zero out the high bits that we want to ignore using the
        // _bzhi_u32() intrinsic.  This ensures only the bytes that are part
        // of the input string participate in the hash value calculation.
        //

        ULONG Last = 0;
        ULONG HighBits;

        //
        // (Sanity check we can math.)
        //

        ASSERT(TrailingBytes >= 1 && TrailingBytes <= 3);

        //
        // Initialize our HighBits to the number of bits in a ULONG (32),
        // then subtract the number of bits represented by TrailingBytes.
        //

        HighBits = sizeof(ULONG) << 3;
        HighBits -= (TrailingBytes << 3);

        //
        // Load the last ULONG, zero out the high bits, then hash.
        //

        Last = _bzhi_u32(DoubleWords[NumberOfDoubleWords], HighBits);
        StringHash = _mm_crc32_u32(StringHash, Last);
    }

    //
    // Wire up the string details.
    //

    String->Hash = StringHash;
    String->Length = Length;
    String->Buffer = (PBYTE)Bytes;

    //
    // Update the caller's bitmap and histogram hash pointers.
    //

    *BitmapHashPointer = BitmapHash;
    *HistogramHashPointer = HistogramHash;

    //
    // Return success.
    //

    return TRUE;
};


RTL_GENERIC_COMPARE_RESULTS
NTAPI
CompareWords(
    PCLONG_STRING LeftString,
    PCLONG_STRING RightString
    )
/*++

Routine Description:

    Compares two words.

Arguments:

    LeftString - Supplies the left word to compare.

    RightString - Supplies the right word to compare.

Return Value:

    GenericLessThan, GenericEqual or GenericGreaterThan depending on the result
    of the comparison.

--*/
{
    ULONG Remaining;
    ULONG LeftStringAlignment;
    ULONG RightStringAlignment;

    LONG Count;
    LONG EqualMask;
    LONG GreaterThanMask;

    PBYTE LeftBuffer;
    PBYTE RightBuffer;

    BYTE LeftTemp[16];
    BYTE RightTemp[16];

    XMMWORD LeftXmm;
    XMMWORD RightXmm;
    XMMWORD EqualXmm;
    XMMWORD GreaterThanXmm;

    YMMWORD LeftYmm;
    YMMWORD RightYmm;
    YMMWORD EqualYmm;
    YMMWORD GreaterThanYmm;

    ASSERT(LeftString->Length == RightString->Length);

    Remaining = LeftString->Length;

    LeftBuffer = (PBYTE)LeftString->Buffer;
    RightBuffer = (PBYTE)RightString->Buffer;

    //
    // We attempt as many 32-byte comparisons as we can, then as many 16-byte
    // comparisons as we can, then a final < 16-byte comparison if necessary.
    //
    // We use aligned loads if possible, falling back to unaligned if not.
    //

StartYmm:

    if (Remaining >= 32) {

        //
        // We have at least 32 bytes to compare for each string.  Check the
        // alignment for each buffer and do an aligned streaming load (non-
        // temporal hint) if our alignment is at a 32-byte boundary or better;
        // reverting to an unaligned load when not.
        //

        LeftStringAlignment = GetAddressAlignment(LeftBuffer);
        RightStringAlignment = GetAddressAlignment(RightBuffer);

        if (LeftStringAlignment < 32) {
            LeftYmm = _mm256_loadu_si256((PYMMWORD)LeftBuffer);
        } else {
            LeftYmm = _mm256_stream_load_si256((PYMMWORD)LeftBuffer);
        }

        if (RightStringAlignment < 32) {
            RightYmm = _mm256_loadu_si256((PYMMWORD)RightBuffer);
        } else {
            RightYmm = _mm256_stream_load_si256((PYMMWORD)RightBuffer);
        }

        //
        // Compare the two vectors.
        //

        EqualYmm = _mm256_cmpeq_epi8(LeftYmm, RightYmm);

        //
        // Generate a mask from the result of the comparison.
        //

        EqualMask = _mm256_movemask_epi8(EqualYmm);

        //
        // There were at least 32 characters remaining in each string buffer,
        // thus, every character needs to have matched in order for this search
        // to continue.  If there were less than 32 characters, we can terminate
        // the search here.  (-1 == 0xffffffff == all bits set == all characters
        // matched.)
        //

        if (EqualMask != -1) {

            //
            // Not all characters were matched.  Determine if the result is
            // greater than or less than and return.
            //

            GreaterThanYmm = _mm256_cmpgt_epi32(LeftYmm, RightYmm);
            GreaterThanMask = _mm256_movemask_epi8(GreaterThanYmm);

            if (GreaterThanMask == -1) {
                return GenericGreaterThan;
            } else {
                return GenericLessThan;
            }
        }

        //
        // All 32 characters were matched.  Update counters and pointers
        // accordingly and jump back to the start of the 32-byte processing.
        //

        Remaining -= 32;

        LeftBuffer += 32;
        RightBuffer += 32;

        goto StartYmm;
    }

    //
    // Intentional follow-on to StartXmm.
    //

StartXmm:

    //
    // Update the search string's alignment.
    //

    if (Remaining >= 16) {

        //
        // We have at least 16 bytes to compare for each string.  Check the
        // alignment for each buffer and do an aligned streaming load (non-
        // temporal hint) if our alignment is at a 16-byte boundary or better;
        // reverting to an unaligned load when not.
        //

        LeftStringAlignment = GetAddressAlignment(LeftBuffer);

        if (LeftStringAlignment < 16) {
            LeftXmm = _mm_loadu_si128((XMMWORD *)LeftBuffer);
        } else {
            LeftXmm = _mm_stream_load_si128((XMMWORD *)LeftBuffer);
        }

        RightXmm = _mm_stream_load_si128((XMMWORD *)RightBuffer);

        //
        // Compare the two vectors.
        //

        EqualXmm = _mm_cmpeq_epi8(LeftXmm, RightXmm);

        //
        // Generate a mask from the result of the comparison.
        //

        EqualMask = _mm_movemask_epi8(EqualXmm);

        //
        // There were at least 16 characters remaining in each string buffer,
        // thus, every character needs to have matched in order for this search
        // to continue.  If there were less than 16 characters, we can terminate
        // this search here.  (-1 == 0xffff -> all bits set -> all characters
        // matched.)
        //

        if (EqualMask != -1) {

            //
            // Not all characters were matched.  Determine if the result is
            // greater than or less than and return.
            //

            GreaterThanXmm = _mm_cmpgt_epi32(LeftXmm, RightXmm);
            GreaterThanMask = _mm_movemask_epi8(GreaterThanXmm);

            if (GreaterThanMask == -1) {
                return GenericGreaterThan;
            } else {
                return GenericLessThan;
            }

        }

        //
        // All 16 characters were matched.  Update counters and pointers
        // accordingly and jump back to the start of the 16-byte processing.
        //

        Remaining -= 16;

        LeftBuffer += 16;
        RightBuffer += 16;

        goto StartXmm;
    }

    if (Remaining == 0) {

        //
        // We'll get here if we successfully matched both strings and all our
        // buffers were aligned (i.e. we don't have a trailing < 16 bytes
        // comparison to perform).
        //

        return GenericEqual;
    }

    //
    // If we get here, we have less than 16 bytes to compare.  Loading the
    // final bytes of each string is a little more complicated, as they could
    // reside within 15 bytes of the end of the page boundary, which would mean
    // that a 128-bit load would cross a page boundary.
    //
    // At best, the page will belong to our process and we'll take a performance
    // hit.  At worst, we won't own the page, and we'll end up triggering a hard
    // page fault.
    //
    // So, see if the buffer addresses plus 16 bytes cross a page boundary.  If
    // they do, take the safe but slower approach of a ranged memcpy (movsb)
    // into a local stack-allocated 16-byte array structure.
    //

    if (!PointerToOffsetCrossesPageBoundary(LeftBuffer, 16)) {

        //
        // No page boundary is crossed, so just do an unaligned 128-bit move
        // into our Xmm register.  (We could do the aligned/unaligned dance
        // here, but it's the last load we'll be doing (i.e. it's not
        // potentially on a loop path), so I don't think it's worth the extra
        // branch cost, although I haven't measured this empirically.)
        //

        LeftXmm = _mm_loadu_si128((XMMWORD *)LeftBuffer);

    } else {

        //
        // We cross a page boundary, so only copy the the bytes we need via
        // __movsb(), then do an aligned stream load into the Xmm register
        // we'll use in the comparison.
        //

        __movsb((PBYTE)LeftTemp, LeftBuffer, Remaining);

        LeftXmm = _mm_stream_load_si128((PXMMWORD)&LeftTemp);
    }

    //
    // Perform the same logic for the right buffer.
    //

    if (!PointerToOffsetCrossesPageBoundary(RightBuffer, 16)) {

        RightXmm = _mm_loadu_si128((XMMWORD *)RightBuffer);

    } else {

        __movsb((PBYTE)RightTemp, RightBuffer, Remaining);

        RightXmm = _mm_stream_load_si128((PXMMWORD)&RightTemp);
    }

    //
    // Compare the final vectors.
    //

    EqualXmm = _mm_cmpeq_epi8(LeftXmm, RightXmm);

    //
    // Generate a mask from the result of the comparison, but mask off (zero
    // out) high bits from the string's remaining length.
    //

    EqualMask = _mm_movemask_epi8(EqualXmm);
    EqualMask = _bzhi_u32(EqualMask, Remaining);

    Count = __popcnt(EqualMask);

    if (Count != Remaining) {

        //
        // Not all characters were matched.  Determine if the result is
        // greater than or less than and return.
        //

        GreaterThanXmm = _mm_cmpgt_epi32(LeftXmm, RightXmm);

        GreaterThanMask = _mm_movemask_epi8(GreaterThanXmm);
        GreaterThanMask = _bzhi_u32(GreaterThanMask, Remaining);

        Count = __popcnt(GreaterThanMask);

        if (Count == Remaining) {
            return GenericGreaterThan;
        } else {
            return GenericLessThan;
        }

    }

    //
    // If we get here, the loop exhausted all values and everything was found
    // to be equal, so return GenericEqual.
    //

    return GenericEqual;
}

_Use_decl_annotations_
BOOLEAN
GetWordStats(
    PDICTIONARY Dictionary,
    PCBYTE Word,
    PWORD_STATS Stats
    )
/*++

Routine Description:

    Retrieves current entry count and maximum entry count statistics about a
    given word in the dictionary, provided it exists.

Arguments:

    Dictionary - Supplies a pointer to a DICTIONARY structure for which the
        word stats are to be obtained.

    Word - Supplies a NULL-terminated array of bytes to obtain stats for from
        the dictionary.  The word must exist in the dictionary.  If it doesn't,
        FALSE is returned.

    Stats - Supplies a pointer to a DICTIONARY_STATS structure that will
        receive the stats information for the given word.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BOOL Success;
    PWORD_STATS WordStats;
    CHARACTER_BITMAP Bitmap;
    DICTIONARY_CONTEXT Context;
    CHARACTER_HISTOGRAM Histogram;
    PWORD_TABLE_ENTRY WordTableEntry;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Word)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Stats)) {
        return FALSE;
    }

    //
    // Zero the context, bitmap and histogram structures.
    //

    ZeroStruct(Context);
    ZeroStruct(Bitmap);
    ZeroStruct(Histogram);

    //
    // Set the TLS context.
    //

    Context.Dictionary = Dictionary;
    DictionaryTlsSetContext(&Context);

    //
    // Acquire the dictionary lock and attempt to find the word.
    //

    AcquireDictionaryLockShared(&Dictionary->Lock);

    Success = FindWordTableEntry(Dictionary,
                                 Word,
                                 &Bitmap,
                                 &Histogram,
                                 &WordTableEntry);

    if (!Success || WordTableEntry == NULL) {

        Success = FALSE;

    } else {

        //
        // Match found!  Write the stats.
        //

        WordStats = &WordTableEntry->WordEntry.Stats;
        Stats->EntryCount = WordStats->EntryCount;
        Stats->MaximumEntryCount = WordStats->MaximumEntryCount;

        Success = TRUE;

    }

    //
    // Release the lock and return our success indicator.
    //

    ReleaseDictionaryLockShared(&Dictionary->Lock);

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
