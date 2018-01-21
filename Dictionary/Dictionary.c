/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Dictionary.c

Abstract:

    This module implements creation and initialization, and destruction routines
    for the dictionary component.  Additionally, it provides concrete function
    entry points for inline functions defined in public headers.

--*/

#include "stdafx.h"

_Use_decl_annotations_
BOOLEAN
CreateDictionary(
    PRTL Rtl,
    PALLOCATOR Allocator,
    DICTIONARY_CREATE_FLAGS CreateFlags,
    PDICTIONARY *DictionaryPointer
    )
/*++

Routine Description:

    Creates and initializes a DICTIONARY structure using the given allocator.

Arguments:

    Rtl - Supplies a pointer to an initialized RTL structure.  This is used to
        obtain function pointers to required NT kernel functions (such as those
        used for AVL tree manipulation).

    Allocator - Supplies a pointer to an initialized ALLOCATOR structure that
        will be used to allocate memory for the underlying DICTIONARY structure.

    CreateFlags - Optionally supplies creation flags that affect the underlying
        behavior of the dictionary.  Currently unused.

    DictionaryPointer - Supplies the address of a variable that will receive
        the address of the newly created DICTIONARY structure if the routine is
        successful (returns TRUE), or NULL if the routine failed.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BOOLEAN Success;
    PDICTIONARY Dictionary;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Rtl)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Allocator)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(DictionaryPointer)) {
        return FALSE;
    }

    //
    // CreateFlags aren't currently used.
    //

    if (CreateFlags.AsULong != 0) {
        return FALSE;
    }

    //
    // Clear the caller's pointer up-front.
    //

    *DictionaryPointer = NULL;

    //
    // Allocate space for the dictionary structure.
    //

    Dictionary = (PDICTIONARY)Allocator->Calloc(Allocator,
                                                1,
                                                sizeof(*Dictionary));

    if (!Dictionary) {
        goto Error;
    }

    //
    // Allocation was successful, continue with initialization.
    //

    Dictionary->SizeOfStruct = sizeof(*Dictionary);
    Dictionary->Rtl = Rtl;
    Dictionary->Allocator = Allocator;
    Dictionary->Flags.AsULong = 0;
    Dictionary->MinimumWordLength = MINIMUM_WORD_LENGTH;
    Dictionary->MaximumWordLength = MAXIMUM_WORD_LENGTH;

    //
    // Use the same allocator for all tables for now.
    //

    Dictionary->BitmapTableAllocator = Allocator;
    Dictionary->HistogramTableAllocator = Allocator;
    Dictionary->WordTableAllocator = Allocator;
    Dictionary->LengthTableAllocator = Allocator;

    //
    // Initialize the dictionary lock, acquire it exclusively, then initialize
    // the underlying AVL tables.  (We acquire and release it to satisfy the SAL
    // _Guarded_by_(Lock) annotation on the underlying BitmapTable element.)
    //

    InitializeDictionaryLock(&Dictionary->Lock);

    AcquireDictionaryLockExclusive(&Dictionary->Lock);

    Rtl->RtlInitializeGenericTableAvl(&Dictionary->BitmapTable.Avl,
                                      BitmapTableCompareRoutine,
                                      BitmapTableAllocateRoutine,
                                      BitmapTableFreeRoutine,
                                      Dictionary);

    Rtl->RtlInitializeGenericTableAvl(&Dictionary->LengthTable.Avl,
                                      LengthTableCompareRoutine,
                                      LengthTableAllocateRoutine,
                                      LengthTableFreeRoutine,
                                      Dictionary);


    ReleaseDictionaryLockExclusive(&Dictionary->Lock);

    //
    // We've completed initialization, indicate success and jump to the end.
    //

    Success = TRUE;

    goto End;

Error:

    Success = FALSE;

    //
    // Intentional follow-on to End.
    //

End:

    //
    // Update the caller's pointer and return.
    //
    // N.B. Dictionary could be NULL here, which is fine.
    //

    *DictionaryPointer = Dictionary;

    return Success;
}

_Use_decl_annotations_
BOOLEAN
DestroyDictionary(
    PDICTIONARY *DictionaryPointer,
    PBOOLEAN IsProcessTerminating
    )
/*++

Routine Description:

    Destroys a previously created DICTIONARY structure, freeing all memory
    unless the IsProcessTerminating flag is TRUE.

Arguments:

    DictionaryPointer - Supplies the address of a variable that contains the
        address of the DICTIONARY structure to destroy.  This variable will be
        cleared (i.e. the pointer will be set to NULL) if the routine destroys
        the dictionary successfully (returns TRUE).

    IsProcessTerminating - Optionally supplies a pointer to a boolean flag
        indicating whether or not the process is terminating.  If the pointer
        is non-NULL and the underlying value is TRUE, the method returns success
        immediately.  (If the process is terminating, there is no need to walk
        the dictionary and all sub-trees freeing each one individually.)

Return Value:

    TRUE on success, FALSE on failure.  If successful, a NULL pointer will be
    written to the DictionaryPointer parameter.

--*/
{
    PRTL Rtl;
    BOOLEAN Success;
    PALLOCATOR Allocator;
    PDICTIONARY Dictionary;
    PWORD_TABLE WordTable;
    PBITMAP_TABLE BitmapTable;
    PHISTOGRAM_TABLE HistogramTable;
    PWORD_TABLE_ENTRY WordTableEntry;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;
    PRTL_ENUMERATE_GENERIC_TABLE_AVL EnumerateTable;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(DictionaryPointer)) {
        goto Error;
    }

    if (ARGUMENT_PRESENT(IsProcessTerminating)) {
        if (*IsProcessTerminating) {

            //
            // Fast-path exit.  Clear the caller's pointer and return success.
            //

            *DictionaryPointer = NULL;
            return TRUE;
        }
    }

    //
    // A valid pointer has been provided, and the process isn't terminating.
    // Initialize aliases and continue with destroy logic.
    //

    Dictionary = *DictionaryPointer;
    Rtl = Dictionary->Rtl;
    Allocator = Dictionary->Allocator;
    EnumerateTable = Rtl->RtlEnumerateGenericTableAvl;

    BitmapTable = &Dictionary->BitmapTable;
    //BitmapAvl = &BitmapTable->Avl;

    //
    // Sanity check the dictionary structure size matches what we expect.
    //

    ASSERT(Dictionary->SizeOfStruct == sizeof(*Dictionary));

    //
    // Acquire the dictionary lock for the duration of the destroy logic.
    //

    AcquireDictionaryLockExclusive(&Dictionary->Lock);


    //
    // Define a helper macro to improve the aesthetics of the nested loop
    // enumeration code.
    //

#define FOR_EACH_ENTRY_IN_TABLE(Name, Type)                                 \
    for (Name##TableEntry = (Type)EnumerateTable(&Name##Table->Avl, TRUE);  \
         Name##TableEntry != NULL;                                          \
         Name##TableEntry = (Type)EnumerateTable(&Name##Table->Avl, FALSE))

    FOR_EACH_ENTRY_IN_TABLE(Bitmap, PBITMAP_TABLE_ENTRY) {

        //
        // Resolve the histogram table for the current bitmap table entry.
        //

        HistogramTable = &BitmapTableEntry->HistogramTable;
        //HistogramAvl = HistogramTable->Avl;

        FOR_EACH_ENTRY_IN_TABLE(Histogram, PHISTOGRAM_TABLE_ENTRY) {

            //
            // Resolve the word table for the current histogram table entry.
            //

            WordTable = &HistogramTableEntry->WordTable;
            //WordAvl = &WordTable->Avl;

            FOR_EACH_ENTRY_IN_TABLE(Word, PWORD_TABLE_ENTRY) {

                NOTHING;

            }
        }
    }

    //
    // Now free the dictionary itself.
    //

    Allocator->FreePointer(Allocator, DictionaryPointer);

    Success = TRUE;

    goto End;

Error:

    Success = FALSE;

    //
    // Intentional follow-on to End.
    //

End:

    return Success;
}

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
    ULONG Index;
    ULONG Length;
    ULONG BitmapHash;
    ULONG StringHash;
    ULONG HistogramHash;
    PLONG Bits;
    PULONG Counts;
    PULONG Doublewords;

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
    // Put stubs in for alignment checks.  (We're not using AVX or AVX2 yet so
    // alignment doesn't matter at the moment.)
    //

    if (!IsAligned(Bitmap, 32)) {
        NOTHING;
    }

    if (!IsAligned(Histogram, 256)) {
        NOTHING;
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
    Doublewords = (PULONG)&Bits;
    BitmapHash = _mm_crc32_u32(BitmapHash, Doublewords[0]);
    BitmapHash = _mm_crc32_u32(BitmapHash, Doublewords[1]);
    BitmapHash = _mm_crc32_u32(BitmapHash, Doublewords[2]);
    BitmapHash = _mm_crc32_u32(BitmapHash, Doublewords[3]);

    //
    // Calculate the histogram hash after a quick sanity check that our number
    // of character bits is divisible by 4.
    //

    ASSERT(!(NUMBER_OF_CHARACTER_BITS % 4));

    HistogramHash = Length;
    for (Index = 0; Index < NUMBER_OF_CHARACTER_BITS; Index += 4) {
        HistogramHash = _mm_crc32_u32(HistogramHash, Counts[Index+0]);
        HistogramHash = _mm_crc32_u32(HistogramHash, Counts[Index+1]);
        HistogramHash = _mm_crc32_u32(HistogramHash, Counts[Index+2]);
        HistogramHash = _mm_crc32_u32(HistogramHash, Counts[Index+3]);
    }

    //
    // Calculate the string hash.
    //

    StringHash = 0;
    Doublewords = (PULONG)Bytes;
    for (Index = 0; Index < Length; Index += 4) {
        StringHash = _mm_crc32_u32(StringHash, Doublewords[Index+0]);
        StringHash = _mm_crc32_u32(StringHash, Doublewords[Index+1]);
        StringHash = _mm_crc32_u32(StringHash, Doublewords[Index+2]);
        StringHash = _mm_crc32_u32(StringHash, Doublewords[Index+3]);
    }

    //
    // Factor in trailing bytes.
    //

    TrailingBytes = Length % 4;
    if (TrailingBytes) {
        ULONG Last = 0;
        PBYTE Dest = (PBYTE)&Last;
        PBYTE Source = (PBYTE)&Doublewords[Length+1];

        for (Index = 0; Index < TrailingBytes; Index++) {
            *Dest++ = Source[Index];
        }

        StringHash = _mm_crc32_u32(StringHash, Last);
    }

    //
    // Add in the string's length to the hash.
    //

    StringHash = _mm_crc32_u32(StringHash, Length);

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

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
