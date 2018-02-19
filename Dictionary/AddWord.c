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
AddWordEntry(
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
        added to the directory.  Set to zero on error.

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
    ULONGLONG StringBytesUsed = 0;
    PULONG BitmapHash;
    PULONG HistogramHash;
    BOOLEAN NewWordEntry;
    BOOLEAN NewLengthEntry;
    BOOLEAN NewBitmapEntry;
    BOOLEAN NewHistogramEntry;
    BOOLEAN IsNewCurrentLongestWord;
    BOOLEAN IsNewLongestWordAllTime;
    PRTL_AVL_TABLE Avl;
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
    PCLONG_STRING CurrentLongestWord;
    PCLONG_STRING LongestWordAllTime;
    PTABLE_ENTRY_HEADER TableEntryHeader;
    ULARGE_INTEGER TotalStringBufferAllocSize;
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
    // Initialize aliases and various table entry pointers.
    //

    Rtl = Dictionary->Rtl;
    WordAllocator = Dictionary->WordAllocator;
    RtlInitializeGenericTableAvl = Rtl->RtlInitializeGenericTableAvl;
    RtlInsertElementGenericTableAvl = Rtl->RtlInsertElementGenericTableAvl;

    WordTableEntry = &WordTableEntryHeader.WordTableEntry;
    LengthTableEntry = &LengthTableEntryHeader.LengthTableEntry;
    BitmapTableEntry = &BitmapTableEntryHeader.BitmapTableEntry;
    HistogramTableEntry = &HistogramTableEntryHeader.HistogramTableEntry;

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
    // table entry's header.  Ditto for the word's length.
    //

    WordTableEntryHeader.Hash = String->Hash;
    LengthTableEntryHeader.Length = String->Length;

    //
    // Initialize the dictionary context and register it with TLS.
    //

    Context.Dictionary = Dictionary;
    if (!DictionaryTlsSetContext(&Context)) {
        goto Error;
    }

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
    // Update the TLS context and initialize the word table alias.
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
    String = &WordEntry->String;

    if (NewWordEntry) {

        //
        // Allocate new space for the underlying string buffer and then copy
        // the input string over such that we're not reliant on the memory
        // provided by the caller.  We add 1 to the length to account for the
        // trailing NULL.
        //

        Length = WordEntry->String.Length;

        Buffer = (PBYTE)WordAllocator->Calloc(WordAllocator, 1, Length + 1);

        if (!Buffer) {
            goto Error;
        }

        //
        // Update the number of bytes allocated to string buffers in the
        // current word table.  Because the high and low parts of the count
        // are split, we need to do some LARGE_INTEGER juggling.
        //

        Avl = &WordTable->Avl;
        TotalStringBufferAllocSize.LowPart = Avl->BytesAllocatedLowPart;
        TotalStringBufferAllocSize.HighPart = Avl->BytesAllocatedHighPart;
        TotalStringBufferAllocSize.QuadPart += (Length + 1);
        Avl->BytesAllocatedLowPart = TotalStringBufferAllocSize.LowPart;
        Avl->BytesAllocatedHighPart = TotalStringBufferAllocSize.HighPart;

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

        //
        // As this is a new word, insert the length into the dictionary's
        // length AVL table.
        //

        LengthTable = &Dictionary->LengthTable;

        Entry = LengthTableEntry;
        EntrySize = sizeof(*LengthTableEntry);
        LengthTableEntry = RtlInsertElementGenericTableAvl(&LengthTable->Avl,
                                                           Entry,
                                                           EntrySize,
                                                           &NewLengthEntry);

        if (!LengthTableEntry) {
            goto Error;
        }

        if (NewLengthEntry) {

            //
            // Initialize the length table entry's linked list head.
            //

            InitializeListHead(&LengthTableEntry->LengthListHead);

            //
            // Copy the word length back over to the length table entry header.
            //

            TableEntryHeader = TABLE_ENTRY_TO_HEADER(LengthTableEntry);
            TableEntryHeader->Length = WordEntry->String.Length;

            //
            // Determine if this is the dictionary's current longest word.
            //

            CurrentLongestWord = Dictionary->Stats.CurrentLongestWord;

            IsNewCurrentLongestWord = (
                CurrentLongestWord == NULL ||
                WordEntry->String.Length > CurrentLongestWord->Length
            );

            if (IsNewCurrentLongestWord) {

                //
                // Register as the new longest word.
                //

                Dictionary->Stats.CurrentLongestWord = &WordEntry->String;

                //
                // Determine if this is the longest word the dictionary has
                // ever seen.
                //

                LongestWordAllTime = Dictionary->Stats.LongestWordAllTime;
                IsNewLongestWordAllTime = (
                    LongestWordAllTime == NULL ||
                    WordEntry->String.Length > LongestWordAllTime->Length
                );

                if (IsNewLongestWordAllTime) {

                    //
                    // If there's already a longest word of all time entry,
                    // we may need to free it if it represents a word that has
                    // since been removed from the dictionary completely.  We
                    // can detect this from the hash value; if it's set to 0,
                    // we use this as an indicator that the word needs to be
                    // freed.
                    //
                    // N.B. We don't need to do this for the current longest
                    //      word as that will always point to an active word.
                    //

                    if (LongestWordAllTime && LongestWordAllTime->Hash == 0) {

                        WordAllocator->FreePointer(
                            WordAllocator,
                            (PPVOID)&Dictionary->Stats.LongestWordAllTime
                        );

                    }

                    //
                    // Register as the new longest word of all time.  We point
                    // to ourselves here again just like we did for the current
                    // longest word registration above.  There is logic in the
                    // word removal routine that checks if the word is the
                    // longest of all time and if so, makes a new copy of it
                    // before removing the original and sets the hash to 0 such
                    // that it can be detected by the code above.
                    //

                    Dictionary->Stats.LongestWordAllTime = &WordEntry->String;
                }
            }
        }

        //
        // Append the word table entry to the length table entry's list head.
        //

        InsertTailList(&LengthTableEntry->LengthListHead,
                       &WordTableEntry->LengthListEntry);

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
    // Update the caller's pointers.
    //

    *WordEntryPointer = WordEntry;

    if (ARGUMENT_PRESENT(EntryCountPointer)) {
        *EntryCountPointer = WordStats->EntryCount;
    }

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

    return Success;
}

_Use_decl_annotations_
BOOLEAN
AddWord(
    PDICTIONARY Dictionary,
    PCBYTE Word,
    PLONGLONG EntryCountPointer
    )
/*++

Routine Description:

    Adds a word to a dictionary if it doesn't exist, increments the word's
    existing count if it does.  Returns the entry count for the word.

    N.B. The word may also be registered as the current and all-time longest
         word associated with the dictionary.

Arguments:

    Dictionary - Supplies a pointer to a DICTIONARY structure to which the
        word is to be added.

    Word - Supplies a NULL-terminated array of bytes to add to the dictionary.
        The length of the array must be between the minimum and maximum lengths
        configured for the dictionary, otherwise the word will be rejected and
        this routine will return FALSE.

    EntryCountPointer - Supplies an address to a variable that receives the
        current entry count associated with the word at the time that it was
        added to the directory.  Set to zero on error.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BOOLEAN Success;
    PWORD_ENTRY WordEntry;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Word)) {
        return FALSE;
    }

    //
    // Obtain an exclusive lock on the dictionary.
    //

    AcquireDictionaryLockExclusive(&Dictionary->Lock);

    Success = AddWordEntry(Dictionary, Word, &WordEntry, EntryCountPointer);

    //
    // Release the lock and return.
    //

    ReleaseDictionaryLockExclusive(&Dictionary->Lock);

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
