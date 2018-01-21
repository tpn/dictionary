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

Arguments:

    TBD.

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
    PLONG_STRING String;
    PWORD_TABLE WordTable;
    PWORD_ENTRY WordEntry;
    PWORD_STATS WordStats;
    PLENGTH_TABLE LengthTable;
    PBITMAP_TABLE BitmapTable;
    PHISTOGRAM_TABLE HistogramTable;
    DICTIONARY_CONTEXT Context;
    PCHARACTER_BITMAP Bitmap;
    PCHARACTER_HISTOGRAM Histogram;
    PRTL_INITIALIZE_GENERIC_TABLE_AVL RtlInitializeGenericTableAvl;
    PRTL_INSERT_ELEMENT_GENERIC_TABLE_AVL RtlInsertElementGenericTableAvl;

    PWORD_TABLE_ENTRY WordTableEntry;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;

    WORD_TABLE_ENTRY_FULL WordTableEntryFull;
    BITMAP_TABLE_ENTRY_FULL BitmapTableEntryFull;
    HISTOGRAM_TABLE_ENTRY_FULL HistogramTableEntryFull;

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
    ZeroStruct(WordTableEntryFull);
    ZeroStruct(BitmapTableEntryFull);
    ZeroStruct(HistogramTableEntryFull);

    //
    // Initialize aliases.
    //

    Rtl = Dictionary->Rtl;
    Allocator = Dictionary->Allocator;
    RtlInitializeGenericTableAvl = Rtl->RtlInitializeGenericTableAvl;
    RtlInsertElementGenericTableAvl = Rtl->RtlInsertElementGenericTableAvl;

    //
    // Initialize pointers.
    //

    String = &WordTableEntryFull.Entry.WordEntry.String;
    Bitmap = &BitmapTableEntryFull.Entry.Bitmap;
    Histogram = &HistogramTableEntryFull.Entry.Histogram;
    BitmapHash = &BitmapTableEntryFull.Hash;
    HistogramHash = &HistogramTableEntryFull.Hash;

    //
    // Initialize the word.  This will verify the length of the incoming string
    // as well as calculate the bitmap and histogram and respective hashes.  We
    // then use these as part of the AVL table insertion.
    //

    Success = InitializeWord(Word,
                             Dictionary->MinimumWordLength,
                             Dictionary->MaximumWordLength,
                             String,
                             Bitmap,
                             Histogram,
                             BitmapHash,
                             HistogramHash);

    if (!Success) {
        return FALSE;
    }

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

    Entry = &BitmapTableEntryFull.Entry;
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
    }

    //
    // Prepare a histogram table entry for potential insertion into the bitmap's
    // histogram table.
    //

    Entry = &HistogramTableEntryFull.Entry;
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
        // Initialize the histogram table's anagram list head.
        //

        InitializeListHead(&HistogramTableEntry->AnagramListHead);
    }

    //
    // Prepare a word table entry for potential insertion into the histogram's
    // word table.
    //

    Entry = &WordTableEntryFull.Entry;
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
        // This is the first time we've seen this word.  Link it to the
        // histogram table's anagram list head.
        //

        InsertTailList(&HistogramTableEntry->AnagramListHead,
                       &WordEntry->AnagramListEntry);

        //
        // Allocate new space for the underlying string buffer and then copy
        // the input string over such that we're not reliant on the memory
        // provided by the caller.  We add 1 to the length to account for the
        // trailing NULL.
        //

        Length = WordEntry->String.Length;

        Buffer = (PBYTE)Allocator->Malloc(Allocator, Length + 1);

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
