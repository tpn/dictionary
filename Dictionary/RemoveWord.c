/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    RemoveWord.c

Abstract:

    This module implements the word removal functionality for the dictionary
    component.

--*/

#include "stdafx.h"

_Use_decl_annotations_
BOOLEAN
RemoveWord(
    PDICTIONARY Dictionary,
    PCBYTE Word,
    LONGLONG *EntryCountPointer
    )
/*++

Routine Description:

    This routine removes an occurrence of an existing word from the dictionary,
    which decrements the count associated with the word.  If the new entry count
    is greater than zero, no further action is taken.  If the word doesn't exist
    in the dictionary, a value of -1 will be returned via the EntryCountPointer
    parameter (N.B. the routine will still return TRUE in this circumstance).

    If the new entry count is zero, the word is removed entirely and all
    associated memory is released.  If the word is registered as either the
    current longest word or the all-time longest word the dictionary has seen,
    a copy will be made of the underlying string and the dictionary stats will
    be updated prior to releasing the original memory.


Arguments:

    Dictionary - Supplies a pointer to a DICTIONARY structure from which the
        word is to be removed.

    Word - Supplies a NULL-terminated array of bytes representing the word to
        remove from the dictionary.  If the word does not exist, this routine
        will return TRUE and write -1 to the EntryCountPointer parameter.

    EntryCountPointer - Supplies the address of a variable that will receive
        the entry count associated with word after removal.  Possible values:

            -1      The word was not found in the dictionary or an error
                    occurred.  The former will be coupled with a return value
                    of TRUE, the latter with a return value of FALSE.

             0      The word was found and has been removed entirely from the
                    dictionary.

            >0      The word was found and the entry count was decremented; the
                    number of remaining references are indicated by this value.

Return Value:

    TRUE on success, FALSE on failure.  Note that TRUE is returned even if the
    word is not found; the caller must test the EntryCountPointer to inspect
    the removal results.

    (FALSE will be returned on parameter validation failure or memory
     allocation failure.)

--*/
{
    PRTL Rtl;
    BOOL Success;
    PBYTE Buffer;
    ULONG AllocSize;
    PLIST_ENTRY Flink;
    PLIST_ENTRY Blink;
    PRTL_AVL_TABLE Avl;
    BOOLEAN ParentIsRoot;
    PCLONG_STRING String;
    PLIST_ENTRY ListHead;
    PLIST_ENTRY ListEntry;
    PWORD_TABLE WordTable;
    PWORD_ENTRY WordEntry;
    PWORD_STATS WordStats;
    PLONG_STRING NewString;
    CHARACTER_BITMAP Bitmap;
    PALLOCATOR WordAllocator;
    PBITMAP_TABLE BitmapTable;
    PLENGTH_TABLE LengthTable;
    DICTIONARY_CONTEXT Context;
    PTABLE_ENTRY_HEADER Parent;
    BOOLEAN IsLengthListEmpty;
    BOOLEAN IsCurrentLongestWord;
    BOOLEAN IsLongestWordAllTime;
    CHARACTER_HISTOGRAM Histogram;
    PCLONG_STRING NextLongestString;
    PHISTOGRAM_TABLE HistogramTable;
    PWORD_TABLE_ENTRY WordTableEntry;
    PWORD_ENTRY NextLongestWordEntry;
    PCLONG_STRING CurrentLongestWord;
    PCLONG_STRING LongestWordAllTime;
    PLENGTH_TABLE_ENTRY LengthTableEntry;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    ULARGE_INTEGER TotalStringBufferAllocSize;
    PTABLE_ENTRY_HEADER LengthTableEntryHeader;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;
    PWORD_TABLE_ENTRY NextLongestWordTableEntry;
    PLENGTH_TABLE_ENTRY NextLongestLengthTableEntry;
    PRTL_DELETE_ELEMENT_GENERIC_TABLE_AVL DeleteElement;
    PTABLE_ENTRY_HEADER NextLongestLengthTableEntryHeader;
    PRTL_NUMBER_GENERIC_TABLE_ELEMENTS_AVL NumberOfElements;
    PTABLE_ENTRY_HEADER NextLongestLengthWordTableEntryHeader;

    //
    // N.B. The splay links and header variables could do with a cleanup.
    //      (They were mostly used for debugging purposes.)
    //

    PRTL_SPLAY_LINKS LengthSplay;
    PRTL_SPLAY_LINKS ParentSplay;
    PRTL_SPLAY_LINKS SubtreeSuccessorSplay;
    PRTL_SPLAY_LINKS SubtreePredecessorSplay;
    PRTL_SPLAY_LINKS RealSuccessorSplay;
    PRTL_SPLAY_LINKS RealPredecessorSplay;

    PTABLE_ENTRY_HEADER SubtreeSuccessorHeader;
    PTABLE_ENTRY_HEADER SubtreePredecessorHeader;
    PTABLE_ENTRY_HEADER RealSuccessorHeader;
    PTABLE_ENTRY_HEADER RealPredecessorHeader;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Word)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(EntryCountPointer)) {
        return FALSE;
    }

    //
    // Zero the context, bitmap and histogram structures.
    //

    ZeroStruct(Context);
    ZeroStruct(Bitmap);
    ZeroStruct(Histogram);

    //
    // Initialize aliases.
    //

    Rtl = Dictionary->Rtl;
    WordAllocator = Dictionary->WordAllocator;
    DeleteElement = Rtl->RtlDeleteElementGenericTableAvl;
    NumberOfElements = Rtl->RtlNumberGenericTableElementsAvl;

    //
    // Write the error indicator (-1) to the caller's pointer up-front.
    //

    *EntryCountPointer = -1;

    //
    // Set the TLS context.  We make heavy use of this toward the end of the
    // routine to potentially delete empty histogram and bitmap table entries.
    //

    Context.Dictionary = Dictionary;
    DictionaryTlsSetContext(&Context);

    //
    // Acquire an exclusive dictionary lock for the duration of this routine.
    //

    AcquireDictionaryLockExclusive(&Dictionary->Lock);

    //
    // Lookup the given word.
    //

    Success = FindWordTableEntry(Dictionary,
                                 Word,
                                 &Bitmap,
                                 &Histogram,
                                 &WordTableEntry);

    if (!Success || WordTableEntry == NULL) {

        //
        // No match found, we're done.  Indicate success and jump to the end.
        // (The caller's entry count pointer will already be set to -1.)
        //

        Success = TRUE;
        goto End;
    }

    //
    // We found a match for the word.  Initialize some additional aliases.
    //

    WordEntry = &WordTableEntry->WordEntry;
    WordStats = &WordEntry->Stats;

    //
    // Invariant check: entry count should be > 0 and the maximum entry count
    // should be greater than or equal to whatever the current entry count is.
    //

    ASSERT(WordStats->EntryCount > 0);
    ASSERT(WordStats->MaximumEntryCount >= WordStats->EntryCount);

    //
    // Decrement the entry count and update the caller's pointer.
    //

    *EntryCountPointer = --WordStats->EntryCount;

    if (WordStats->EntryCount > 0) {

        //
        // There are additional references remaining to this word, we don't
        // need to do anything else for now.  Jump to the end.
        //

        Success = TRUE;
        goto End;
    }

    //
    // There are no more references to this word, so it can be completely
    // removed from the dictionary.
    //

    String = &WordEntry->String;
    CurrentLongestWord = Dictionary->Stats.CurrentLongestWord;
    LongestWordAllTime = Dictionary->Stats.LongestWordAllTime;

    ListEntry = &WordTableEntry->LengthListEntry;
    Flink = ListEntry->Flink;
    Blink = ListEntry->Blink;

    IsLengthListEmpty = RemoveEntryList(ListEntry);

    IsCurrentLongestWord = (String == CurrentLongestWord);
    IsLongestWordAllTime = (String == LongestWordAllTime);

    LengthTable = &Dictionary->LengthTable;

    //
    // Invariant check: if we're the longest word of all time, we must be the
    // current longest word, too.
    //

    if (IsLongestWordAllTime) {
        ASSERT(IsCurrentLongestWord);
    }

    if (IsLengthListEmpty) {

        ASSERT(Flink->Blink == Blink && Blink->Flink == Flink);
        ListHead = Flink;
        LengthTableEntry = CONTAINING_RECORD(ListHead,
                                             LENGTH_TABLE_ENTRY,
                                             LengthListHead);
        LengthTableEntryHeader = TABLE_ENTRY_TO_HEADER(LengthTableEntry);

        LengthSplay = &LengthTableEntryHeader->SplayLinks;

        Parent = (PTABLE_ENTRY_HEADER)LengthTableEntryHeader->Parent;
        ParentSplay = &Parent->SplayLinks;

        ParentIsRoot = RtlIsRoot(ParentSplay);

        SubtreeSuccessorSplay = Rtl->RtlSubtreeSuccessor(LengthSplay);
        SubtreePredecessorSplay = Rtl->RtlSubtreePredecessor(LengthSplay);

        RealSuccessorSplay = Rtl->RtlRealSuccessor(LengthSplay);
        RealPredecessorSplay = Rtl->RtlRealPredecessor(LengthSplay);

        SubtreeSuccessorHeader = (PTABLE_ENTRY_HEADER)SubtreeSuccessorSplay;
        SubtreePredecessorHeader = (PTABLE_ENTRY_HEADER)SubtreePredecessorSplay;

        RealSuccessorHeader = (PTABLE_ENTRY_HEADER)RealSuccessorSplay;
        RealPredecessorHeader = (PTABLE_ENTRY_HEADER)RealPredecessorSplay;

        //
        // Invariant check: if we're the current longest word, there shouldn't
        // be any length successors, and if there are any length successors,
        // we shouldn't be the longest current word.
        //

        if (IsCurrentLongestWord) {
            ASSERT(RealSuccessorHeader == NULL);
        } else if (RealSuccessorHeader != NULL) {
            ASSERT(!IsCurrentLongestWord);
        }

        if (NumberOfElements(&LengthTable->Avl) == 1) {

            //
            // If there is only one element left in the length table, our
            // parent should be the root.
            //

            ASSERT(ParentIsRoot);
            ASSERT(RealPredecessorSplay == ParentSplay);

            //
            // This was the last word in the table, clear all of the next
            // longest variables.
            //

            NextLongestLengthTableEntryHeader = NULL;
            NextLongestWordTableEntry = NULL;
            NextLongestString = NULL;

            if (IsCurrentLongestWord) {

                //
                // Clear the dictionary's current longest word pointer.
                //

                Dictionary->Stats.CurrentLongestWord = NULL;
            }

        } else if (IsCurrentLongestWord) {

            //
            // Our predecessor should be the next longest length.
            //

            NextLongestLengthTableEntryHeader = RealPredecessorHeader;

            //
            // Verify the length is less than our length.
            //

            ASSERT(NextLongestLengthTableEntryHeader->Length < String->Length);

            //
            // Get the linked-list head of the next longest length word entry,
            // verify it's not empty (we should never have empty length lists),
            // and then get the first entry, which we'll promote to the next
            // longest word entry.
            //

            NextLongestLengthTableEntry =
                &NextLongestLengthTableEntryHeader->LengthTableEntry;

            ListHead = &NextLongestLengthTableEntry->LengthListHead;

            //
            // We should never have an empty length list.
            //

            ASSERT(!IsListEmpty(ListHead));

            //
            // Grab the first entry.
            //

            ListEntry = ListHead->Flink;

            //
            // Convert it into a word table entry, then resolve the remaining
            // aliases.
            //

            NextLongestWordTableEntry = CONTAINING_RECORD(ListEntry,
                                                          WORD_TABLE_ENTRY,
                                                          LengthListEntry);

            NextLongestWordEntry = &NextLongestWordTableEntry->WordEntry;
            NextLongestString = &NextLongestWordEntry->String;

            //
            // Final sanity check the length is less than our length.
            //

            ASSERT(NextLongestString->Length < String->Length);

            //
            // Update the dictionary's current longest word.
            //

            Dictionary->Stats.CurrentLongestWord = NextLongestString;
        }

        //
        // Now that we've obtained the next longest entry to promote (if
        // applicable), we can delete our length table entry.
        //

        Success = Rtl->RtlDeleteElementGenericTableAvl(&LengthTable->Avl,
                                                       LengthTableEntry);

        //
        // Ensure the deletion was successful and then clear our pointer.
        //

        ASSERT(Success);
        LengthTableEntry = NULL;

    } else if (IsCurrentLongestWord) {

        //
        // The next entry in the length linked-list will be promoted to the
        // current longest entry.  As there are still entries remaining with
        // this length we don't need to delete any length table entries.
        //

        NextLongestWordTableEntry = CONTAINING_RECORD(Flink,
                                                      WORD_TABLE_ENTRY,
                                                      LengthListEntry);

        NextLongestLengthWordTableEntryHeader =
            TABLE_ENTRY_TO_HEADER(NextLongestWordTableEntry);

        NextLongestWordEntry = &NextLongestWordTableEntry->WordEntry;
        NextLongestString = &NextLongestWordEntry->String;

        NextLongestString = &NextLongestWordEntry->String;
        ASSERT(NextLongestString->Length == String->Length);

        //
        // Update the dictionary's current longest word to this next entry.
        //

        Dictionary->Stats.CurrentLongestWord = NextLongestString;
    }

    if (IsLongestWordAllTime) {

        //
        // We need to make a copy of ourselves such that we can continue
        // persisting as the longest entry after our removal.
        //

        //
        // Account for the trailing NULL via the + 1.
        //

        AllocSize = sizeof(LONG_STRING) + (String->Length + 1);
        Buffer = (PBYTE)WordAllocator->Calloc(WordAllocator, 1, AllocSize);
        if (!Buffer) {
            goto Error;
        }

        //
        // Carve out the new long string structure.  We explicitly set the hash
        // to 0 as an indicator to the AddWord() routine that this memory needs
        // to be explicitly freed if a new longest word is added.
        //

        NewString = (PLONG_STRING)Buffer;
        Buffer += sizeof(LONG_STRING);

        NewString->Hash = 0;
        NewString->Length = String->Length;
        NewString->Buffer = Buffer;
        Buffer += String->Length;

        //
        // Sanity check our buffer address logic.
        //

        ASSERT(Buffer == RtlOffsetToPointer(NewString, AllocSize - 1));

        //
        // Copy over the underlying string.
        //

        CopyMemory(NewString->Buffer, String->Buffer, String->Length);

        //
        // Explicitly set the trailing NULL.
        //

        NewString->Buffer[String->Length] = '\0';

        //
        // Update the dictionary to point to the new string.
        //

        Dictionary->Stats.LongestWordAllTime = NewString;
    }

    //
    // Once we get here, we're ready to remove the word from the table.  This
    // also requires checking to see if we were the last word table entry for
    // the histogram, and if the histogram was the last entry for the bitmap.
    // If so, we need to clean up all those entries too.
    //

    //
    // Initialize table and entry aliases.
    //

    WordTable = Context.WordTable;
    BitmapTable = &Dictionary->BitmapTable;
    HistogramTable = Context.HistogramTable;
    BitmapTableEntry = Context.BitmapTableEntry;
    HistogramTableEntry = Context.HistogramTableEntry;

    //
    // Delete the word table entry.
    //

    if (!DeleteElement(&WordTable->Avl, WordTableEntry)) {
        goto Error;
    }

    //
    // Free the underlying string buffer.
    //

    WordAllocator->FreePointer(WordAllocator, (PPVOID)&String->Buffer);

    //
    // Update the number of bytes allocated to string buffers in the
    // current word table.  Because the high and low parts of the count
    // are split, we need to do some LARGE_INTEGER juggling.
    //

    Avl = &WordTable->Avl;
    TotalStringBufferAllocSize.LowPart = Avl->BytesAllocatedLowPart;
    TotalStringBufferAllocSize.HighPart = Avl->BytesAllocatedHighPart;
    TotalStringBufferAllocSize.QuadPart -= (String->Length + 1);
    Avl->BytesAllocatedLowPart = TotalStringBufferAllocSize.LowPart;
    Avl->BytesAllocatedHighPart = TotalStringBufferAllocSize.HighPart;

    if (NumberOfElements(Avl) > 0) {

        //
        // We shouldn't have a 0 string buffer alloc size if there are
        // still elements in the table.
        //

        ASSERT(TotalStringBufferAllocSize.QuadPart > 0);

    } else {

        //
        // Likewise, if there are no more elements, the buffer size should
        // also indicate 0 bytes.
        //

        ASSERT(TotalStringBufferAllocSize.QuadPart == 0);

        //
        // The histogram table entry has no more words, so it can be deleted.
        //

        if (!DeleteElement(&HistogramTable->Avl, HistogramTableEntry)) {
            goto Error;
        }

        //
        // If the histogram table has no more entries, the bitmap entry can
        // be deleted.
        //

        if (NumberOfElements(&HistogramTable->Avl) == 0) {

            if (!DeleteElement(&BitmapTable->Avl, BitmapTableEntry)) {
                goto Error;
            }
        }
    }

    //
    // We're finally finished, indicate success and return.
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
    // Release our exclusive lock and return the success indicator.
    //

    ReleaseDictionaryLockExclusive(&Dictionary->Lock);

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
