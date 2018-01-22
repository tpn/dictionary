/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    AddWord.c

Abstract:

    This module implements the add word functionality for the dictionary
    component.

--*/

#include "stdafx.h"

_Use_decl_annotations_
BOOLEAN
AddWord(
    PDICTIONARY Dictionary,
    PCBYTE Word,
    PCWORD_ENTRY *WordEntryPointer,
    PLONGLONG EntryCountPointer
    )
/*++

Routine Description:

    Adds a word to a dictionary if it doesn't exist, increments the word's
    existing count if it does.  Returns the word entry for the word and the
    entry count at the time it was added (i.e. if this is 1, indicates it was
    the first entry added to the table).

    N.B. The word may also be registered as the current and all-time longest
         word associated with the dictionary.

Arguments:

    Dictionary - Supplies a pointer to a DICTIONARY structure to which the
        word is to be added.

    Word - Supplies a NULL-terminated array of bytes to add to the dictionary.
        The length of the array must be between the minimum and maximum lengths
        configured for the dictionary, otherwise the word will be rejected and
        this routine will return FALSE.

    WordEntryPointer - Supplies an address to a variable that receives the
        address of the WORD_ENTRY structure representing the word added if
        no error occurred.  Will be set to NULL on error.

    EntryCountPointer - Supplies an address to a variable that receives the
        current entry count associated with the word at the time that it was
        added to the directory.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    PRTL Rtl;
    BOOL Success;
    PVOID Entry;
    PBYTE Buffer;
    ULONG Length;
    ULONG EntrySize;
    PULONG BitmapHash;
    PULONG HistogramHash;
    BOOLEAN NewWordEntry;
    BOOLEAN NewBitmapEntry;
    BOOLEAN NewHistogramEntry;
    PALLOCATOR Allocator;
    PALLOCATOR WordAllocator;
    PLONG_STRING String;
    PWORD_TABLE WordTable;
    PWORD_ENTRY WordEntry;
    PWORD_STATS WordStats;
    PLENGTH_TABLE LengthTable;
    PBITMAP_TABLE BitmapTable;
    DICTIONARY_CONTEXT Context;
    CHARACTER_BITMAP Bitmap;
    CHARACTER_HISTOGRAM Histogram;
    PHISTOGRAM_TABLE HistogramTable;
    PTABLE_ENTRY_HEADER TableEntryHeader;
    PRTL_INITIALIZE_GENERIC_TABLE_AVL RtlInitializeGenericTableAvl;
    PRTL_INSERT_ELEMENT_GENERIC_TABLE_AVL RtlInsertElementGenericTableAvl;

    PWORD_TABLE_ENTRY WordTableEntry;
    PLENGTH_TABLE_ENTRY LengthTableEntry;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;

    TABLE_ENTRY_HEADER WordTableEntryHeader;
    TABLE_ENTRY_HEADER LengthTableEntryHeader;
    TABLE_ENTRY_HEADER BitmapTableEntryHeader;
    TABLE_ENTRY_HEADER HistogramTableEntryHeader;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Word)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(WordEntryPointer)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(EntryCountPointer)) {
        return FALSE;
    }

    //
    // Clear the caller's word entry and entry count pointers up-front.
    //

    *WordEntryPointer = NULL;
    *EntryCountPointer = 0;

    //
    // Zero local structures.
    //

    ZeroStruct(Context);
    ZeroStruct(WordTableEntryHeader);
    ZeroStruct(LengthTableEntryHeader);
    ZeroStruct(BitmapTableEntryHeader);
    ZeroStruct(HistogramTableEntryHeader);

    //
    // Initialize aliases.
    //

    Rtl = Dictionary->Rtl;
    Allocator = Dictionary->Allocator;
    WordAllocator = Dictionary->WordAllocator;
    RtlInitializeGenericTableAvl = Rtl->RtlInitializeGenericTableAvl;
    RtlInsertElementGenericTableAvl = Rtl->RtlInsertElementGenericTableAvl;

    //
    // Initialize pointers.
    //

    WordTableEntry = HEADER_TO_WORD_TABLE_ENTRY(&WordTableEntryHeader);
    LengthTableEntry = HEADER_TO_LENGTH_TABLE_ENTRY(&LengthTableEntryHeader);
    BitmapTableEntry = HEADER_TO_BITMAP_TABLE_ENTRY(&BitmapTableEntryHeader);
    HistogramTableEntry =
        HEADER_TO_HISTOGRAM_TABLE_ENTRY(&HistogramTableEntryHeader);

    String = &WordTableEntry->WordEntry.String;
    BitmapHash = &BitmapTableEntryHeader.Hash;
    HistogramHash = &HistogramTableEntryHeader.Hash;

    //
    // Initialize the word.  This will verify the length of the incoming string
    // as well as calculate the bitmap and histogram and respective hashes.  We
    // then use these as part of the AVL table insertion.
    //

    Success = InitializeWord(Word,
                             Dictionary->MinimumWordLength,
                             Dictionary->MaximumWordLength,
                             String,
                             &Bitmap,
                             &Histogram,
                             BitmapHash,
                             HistogramHash);

    if (!Success) {
        return FALSE;
    }

    //
    // Copy the word's hash into the appropriate location within the word
    // table entry's header.
    //

    WordTableEntryHeader.Hash = String->Hash;

    //
    // Initialize the dictionary context and register it with TLS.
    //

    Context.Dictionary = Dictionary;
    DictionaryTlsSetContext(&Context);

    //
    // Acquire the exclusive dictionary lock for the remaining duration of the
    // routine.
    //

    AcquireDictionaryLockExclusive(&Dictionary->Lock);

    //
    // Prepare a bitmap table entry for potential insertion into the bitmap
    // AVL table.
    //

    Entry = BitmapTableEntry;
    EntrySize = sizeof(*BitmapTableEntry);
    BitmapTable = &Dictionary->BitmapTable;

    BitmapTableEntry = RtlInsertElementGenericTableAvl(&BitmapTable->Avl,
                                                       Entry,
                                                       EntrySize,
                                                       &NewBitmapEntry);

    if (!BitmapTableEntry) {
        goto Error;
    }

    //
    // Update the TLS context and initialize the histogram table alias.
    //

    Context.BitmapTableEntry = BitmapTableEntry;
    HistogramTable = &BitmapTableEntry->HistogramTable;

    if (NewBitmapEntry) {

        //
        // A new bitmap entry was created; initialize the underlying histogram
        // AVL table.
        //

        RtlInitializeGenericTableAvl(&HistogramTable->Avl,
                                     HistogramTableCompareRoutine,
                                     HistogramTableAllocateRoutine,
                                     HistogramTableFreeRoutine,
                                     Dictionary);

        //
        // Copy the hash back over, it won't be done automatically for us as
        // we're embedding it within the RTL_BALANCED_LINKS structure.
        //

        TableEntryHeader = TABLE_ENTRY_TO_HEADER(BitmapTableEntry);
        TableEntryHeader->Hash = *BitmapHash;
    }

    //
    // Prepare a histogram table entry for potential insertion into the bitmap's
    // histogram table.
    //

    Entry = HistogramTableEntry;
    EntrySize = sizeof(*HistogramTableEntry);
    HistogramTableEntry = RtlInsertElementGenericTableAvl(&HistogramTable->Avl,
                                                          Entry,
                                                          EntrySize,
                                                          &NewHistogramEntry);

    if (!HistogramTableEntry) {

        //
        // N.B. We should probably free the bitmap table entry and remove it
        //      from the dictionary if NewBitmapEntry == TRUE.  There's no harm
        //      in leaving it for now, though, as we don't have an invariant
        //      that requires nodes to have at least one child.
        //
        //      (Also, it's easier.)
        //

        goto Error;
    }

    //
    // Update the TLS context and initialize the word entry table alias.
    //

    Context.HistogramTableEntry = HistogramTableEntry;
    WordTable = &HistogramTableEntry->WordTable;

    if (NewHistogramEntry) {

        //
        // A new histogram entry was created; initialize the underlying word
        // entry AVL table.
        //

        RtlInitializeGenericTableAvl(&WordTable->Avl,
                                     WordTableCompareRoutine,
                                     WordTableAllocateRoutine,
                                     WordTableFreeRoutine,
                                     Dictionary);

        //
        // Copy the hash back over.
        //

        TableEntryHeader = TABLE_ENTRY_TO_HEADER(HistogramTableEntry);
        TableEntryHeader->Hash = *HistogramHash;
    }

    //
    // Prepare a word table entry for potential insertion into the histogram's
    // word table.
    //

    Context.String = String;
    Context.WordTable = WordTable;
    Context.WordEntry = &WordTableEntry->WordEntry;
    Context.WordTableEntry = WordTableEntry;
    Context.TableEntryHeader = &WordTableEntryHeader;

    Entry = WordTableEntry;
    EntrySize = sizeof(*WordTableEntry);
    WordTableEntry = RtlInsertElementGenericTableAvl(&WordTable->Avl,
                                                     Entry,
                                                     EntrySize,
                                                     &NewWordEntry);

    if (!WordTableEntry) {

        //
        // See explanation above in comment regarding failed histogram table
        // entry.  TL;DR ignore for now.
        //

        goto Error;
    }

    WordEntry = &WordTableEntry->WordEntry;

    if (NewWordEntry) {

        //
        // Allocate new space for the underlying string buffer and then copy
        // the input string over such that we're not reliant on the memory
        // provided by the caller.  We add 1 to the length to account for the
        // trailing NULL.
        //

        Length = WordEntry->String.Length;

        Buffer = (PBYTE)WordAllocator->Malloc(Allocator, Length + 1);

        if (!Buffer) {
            goto Error;
        }

        //
        // Copy the buffer, add the trailing NULL, switch the underlying word
        // entry's buffer pointer.
        //

        CopyMemory(Buffer, WordEntry->String.Buffer, Length);
        Buffer[Length] = '\0';
        WordEntry->String.Buffer = Buffer;

        //
        // Copy the hash back over.
        //

        TableEntryHeader = TABLE_ENTRY_TO_HEADER(WordTableEntry);
        TableEntryHeader->Hash = WordEntry->String.Hash;

    } else {

        //
        // This word has already been added to the dictionary, nothing more
        // needs to be done here.  (The count is incremented below.)
        //

        NOTHING;

    }

    //
    // Increment the word's entry count and capture the current value.  Update
    // the maximum entry count if applicable.
    //

    WordStats = &WordEntry->Stats;
    WordStats->EntryCount++;

    if (WordStats->EntryCount > WordStats->MaximumEntryCount) {

        //
        // We've found a new maximum; update the counter accordingly.
        //

        WordStats->MaximumEntryCount = WordStats->EntryCount;
    }

    //
    // TODO: handle the length-tracking requirements.
    //

    LengthTable = &Dictionary->LengthTable;

    //
    // Update the caller's pointers.
    //

    *WordEntryPointer = WordEntry;
    *EntryCountPointer = WordStats->EntryCount;

    //
    // Indicate success and jump to the end, we're done.
    //

    Success = TRUE;

    goto End;

 Error:

    Success = FALSE;

    //
    // Intentional follow-on to End.
    //

End:

    ReleaseDictionaryLockExclusive(&Dictionary->Lock);

    DictionaryTlsSetContext(NULL);

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
