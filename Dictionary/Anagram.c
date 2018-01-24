/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Anagram.c

Abstract:

    This module implements functionality related to the anagram functionality
    of the dictionary component.  Routines are provided for retrieving a list
    of word entry anagrams from a dictionary.

--*/

#include "stdafx.h"

_Use_decl_annotations_
BOOLEAN
NTAPI
GetWordAnagrams(
    PDICTIONARY Dictionary,
    PALLOCATOR Allocator,
    PCBYTE Word,
    PLINKED_WORD_LIST *LinkedWordListPointer
    )
/*++

Routine Description:

    Constructs a list of word entries that are anagrams of a given word entry.

Arguments:

    Dictionary - Supplies a pointer to a DICTIONARY structure for which the
        anagrams are to be retrieved.

    Allocator - Supplies a pointer to an ALLOCATOR structure that will be used
        to allocate the memory that backs the address returned via the param
        LinkedWordListPointer and all associated LINKED_WORD_ENTRY items.

    Word - Supplies a pointer to an array of bytes of an existing word in the
        dictionary for which anagrams are to be obtained.

    LinkedWordListPointer - Supplies the address of a variable that receives
        the address of a LINKED_WORD_LIST structure (allocated via Allocator)
        if there is at least one anagram for the given word entry.  If there
        are no anagrams, a NULL pointer is returned.  The pointer must be freed
        via the Allocator once the user has finished with the structure.

Return Value:

    TRUE on success, FALSE on failure.  If the word does not exist in the
    dictionary, FALSE will be returned.  If the word *does* exist in the
    dictionary, but it has no anagrams, TRUE will be returned, but the
    LinkedWordListPointer will be NULL.

--*/
{
    PRTL Rtl;
    BYTE Byte;
    PCBYTE Bytes;
    ULONG Index;
    ULONG Count = 0;
    ULONG Total;
    ULONG Length;
    PULONG Counts;
    PBYTE Buffer;
    PBYTE StructBuffer;
    PBYTE StringBuffer;
    PBYTE ExpectedStructBuffer;
    PBYTE ExpectedStringBuffer;
    BOOLEAN Success;
    PWORD_STATS Stats;
    PANAGRAM_LIST Anagrams;
    LARGE_INTEGER AllocSize;
    LARGE_INTEGER StringBufferAllocSize;
    ULONGLONG StringBytesUsed = 0;
    DICTIONARY_CONTEXT Context;
    PCLONG_STRING String;
    PLONG_STRING NewString;
    PWORD_TABLE WordTable;
    PWORD_ENTRY WordEntry;
    PWORD_ENTRY NewWordEntry;
    PCLONG_STRING SourceString;
    PWORD_ENTRY SourceWordEntry;
    PWORD_TABLE_ENTRY WordTableEntry;
    PWORD_TABLE_ENTRY SourceWordTableEntry;
    PLINKED_WORD_ENTRY LinkedWordEntry;
    CHARACTER_BITMAP SourceBitmap;
    CHARACTER_HISTOGRAM Histogram;
    CHARACTER_HISTOGRAM SourceHistogram;
    RTL_GENERIC_COMPARE_RESULTS Comparison;
    PRTL_ENUMERATE_GENERIC_TABLE_AVL EnumerateTable;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Allocator)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Word)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(LinkedWordListPointer)) {
        return FALSE;
    }

    //
    // Clear the caller's pointer up-front.
    //

    *LinkedWordListPointer = NULL;

    //
    // Zero the context, source bitmap and histogram structures.
    //

    ZeroStruct(Context);
    ZeroStruct(SourceBitmap);
    ZeroStruct(SourceHistogram);

    //
    // Initialize aliases.
    //

    Rtl = Dictionary->Rtl;
    EnumerateTable = Rtl->RtlEnumerateGenericTableAvl;

    //
    // Initialize the dictionary context and register it with TLS.
    //

    Context.Dictionary = Dictionary;
    DictionaryTlsSetContext(&Context);

    //
    // Acquire a shared lock for the duration of this routine.
    //

    AcquireDictionaryLockShared(&Dictionary->Lock);

    //
    // Find the word table entry for the given word.
    //

    Success = FindWordTableEntry(Dictionary,
                                 Word,
                                 &SourceBitmap,
                                 &SourceHistogram,
                                 &SourceWordTableEntry);

    if (!Success || !SourceWordTableEntry) {

        //
        // An internal error occurred or there was no such word.
        //

        goto Error;
    }

    WordTable = Context.WordTable;

    //
    // Get the number of words (i.e. potential anagrams) from this word table.
    //

    Total = Rtl->RtlNumberGenericTableElementsAvl(&WordTable->Avl);

    if (Total <= 1) {

        //
        // No anagrams present for this word.  Return success.  (Note we will
        // have already cleared the caller's LinkedWordListPointer by this
        // stage, indicating that no anagrams were found.)
        //

        Success = TRUE;
        goto End;
    }

    //
    // There are at least two word table entries sharing this histogram.
    // Optimistically allocate enough space to store every word and supporting
    // structures, which assumes there are no collisions.
    //
    // If there are collisions, worst case scenario we allocate more memory
    // than necessary.  However, as the caller provides the allocator, we can
    // pass that burden off to them; it doesn't really affect us.
    //

    //
    // The word table will track the total number of bytes allocated for
    // string buffers for all words in the table.  We start off with that as
    // our allocation size.
    //

    AllocSize.LowPart = WordTable->Avl.BytesAllocatedLowPart;
    AllocSize.HighPart = WordTable->Avl.BytesAllocatedHighPart;

    //
    // Take a copy of this value for some sanity checks later.
    //

    StringBufferAllocSize.QuadPart = AllocSize.QuadPart;

    //
    // Initialize additional aliases.
    //

    SourceWordEntry = &SourceWordTableEntry->WordEntry;
    SourceString = &SourceWordEntry->String;

    //
    // Now factor in the overhead for all the supporting structures.
    //

    AllocSize.QuadPart += (

        //
        // Account for the containing ANAGRAM_LIST structure.
        //

        sizeof(ANAGRAM_LIST) +

        //
        // Account for a LINKED_WORD_ENTRY for every entry present in the
        // histogram table.
        //

        (sizeof(LINKED_WORD_ENTRY) * Total)

    );

    //
    // Allocate a buffer for the final size.
    //

    Buffer = Allocator->Calloc(Allocator, 1, AllocSize.QuadPart);
    if (!Buffer) {
        goto Error;
    }

    //
    // Buffer was allocated successfully.  Carve out the ANAGRAM_LIST structure
    // and initialize the list head.
    //

    Anagrams = (PANAGRAM_LIST)Buffer;

    //
    // Subsequent struct buffers (for LINKED_WORD_ENTRY structs) will be carved
    // out from the StructBuffer.  String buffers for the underlying words will
    // start at the end of all the structures.  This allows us to maintain the
    // struct alignment (which will need to be in multiples of 8).
    //

    StructBuffer = Buffer + sizeof(ANAGRAM_LIST);
    StringBuffer = Buffer + (sizeof(LINKED_WORD_ENTRY) * Total);

    InitializeListHead(&Anagrams->ListHead);

    //
    // Enumerate all words in the table and generate a histogram, then compare
    // it to the histogram of our incoming string.  If they match, we've found
    // an anagram, so add it to the list.
    //

    FOR_EACH_ENTRY_IN_TABLE(Word, PWORD_TABLE_ENTRY) {

        //
        // Resolve the word entry, underlying string and stats from the word
        // table entry that was just resolved.
        //

        WordEntry = &WordTableEntry->WordEntry;
        String = &WordEntry->String;
        Length = String->Length;
        Stats = &WordEntry->Stats;

        //
        // Was this our source string?  The addresses will match up if so, and
        // we can omit it from the comparison.
        //

        if (String == SourceString) {
            continue;
        }

        //
        // If the lengths don't match, this can't possibly be an anagram.  It
        // also implies a collision.
        //

        if (String->Length != SourceString->Length) {
            Dictionary->LengthCollisions++;
            continue;
        }

        //
        // Increment our entry counter
        //

        Count++;

        //
        // Clear our local histogram buffer, then loop over the word and create
        // a new histogram.
        //

        ZeroStruct(Histogram);

        Bytes = (PCBYTE)String->Buffer;
        Counts = (PULONG)&Histogram.Counts;
        for (Index = 0; Index < Length; Index++) {
            Byte = Bytes[Index];
            Counts[Byte]++;
        }

        //
        // Compare the histogram.
        //

        Comparison = CompareHistogramsAlignedAvx2(&Histogram, &SourceHistogram);
        if (Comparison != GenericEqual) {

            //
            // Histogram collision!  Skip this entry.
            //

            Dictionary->HistogramCollisions++;
            continue;
        }

        //
        // The histogram matches, therefore this is a valid anagram.  Carve
        // out the relevant structures from our buffer and wire everything
        // up to the anagram list.
        //

        LinkedWordEntry = (PLINKED_WORD_ENTRY)StructBuffer;
        StructBuffer += sizeof(LINKED_WORD_ENTRY);

        NewWordEntry = &LinkedWordEntry->WordEntry;
        NewString = &NewWordEntry->String;

        //
        // Initialize the length, hash, and word stats.
        //

        NewString->Length = Length;
        NewString->Hash = String->Hash;
        NewWordEntry->Stats.EntryCount = Stats->EntryCount;
        NewWordEntry->Stats.MaximumEntryCount = Stats->MaximumEntryCount;

        //
        // Carve out the string buffer and copy the string over.
        //

        NewString->Buffer = (PBYTE)StringBuffer;
        StringBuffer += (Length + 1);
        StringBytesUsed += (Length + 1);

        CopyMemory(NewString->Buffer, String->Buffer, Length);

        //
        // Add the entry to the anagram list and increment the count.
        //

        InsertTailList(&Anagrams->ListHead, &LinkedWordEntry->ListEntry);
        Anagrams->NumberOfEntries++;
    }

    //
    // Sanity check buffer addresses.  StructBuffer should point to the end of
    // the structures (which also happens to be the start of the string buffers)
    // and StringBuffer should point to the number of bytes past the initial
    // string buffer according to how many bytes we recorded using in the loop
    // above.
    //

    ExpectedStructBuffer = (
        Buffer +
        sizeof(ANAGRAM_LIST) +
        (sizeof(LINKED_WORD_ENTRY) * Anagrams->NumberOfEntries)
    );

    ExpectedStringBuffer = (
        Buffer +
        (sizeof(LINKED_WORD_ENTRY) * Total) +
        StringBytesUsed
    );

    ASSERT(StructBuffer == ExpectedStructBuffer);
    ASSERT(StringBuffer == ExpectedStringBuffer);

    //
    // We've enumerated all words at the given histogram.  If no entries were
    // registered as anagrams, we can free the allocated structure now.
    //

    if (Anagrams->NumberOfEntries == 0) {

        Allocator->FreePointer(Allocator, &Anagrams);

    } else {

        //
        // Otherwise, update the caller's pointer with the anagram.
        //

        *LinkedWordListPointer = &Anagrams->LinkedWordList;
    }

    //
    // We're done, indicate success and finish up.
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
    // Release the lock and return the success value.
    //

    ReleaseDictionaryLockShared(&Dictionary->Lock);

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
