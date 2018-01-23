/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    FindWord.c

Abstract:

    This module implements the find word functionality for the dictionary
    component.

--*/

#include "stdafx.h"

_Use_decl_annotations_
BOOLEAN
FindWordTableEntry(
    PDICTIONARY Dictionary,
    PCBYTE Word,
    PCHARACTER_BITMAP Bitmap,
    PCHARACTER_HISTOGRAM Histogram,
    PWORD_TABLE_ENTRY *WordTableEntryPointer
    )
/*++

Routine Description:

    TBD.

Arguments:

    TBD.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    PRTL Rtl;
    BOOLEAN Success;
    PULONG BitmapHash;
    PULONG HistogramHash;
    PLONG_STRING String;
    PWORD_TABLE WordTable;
    PBITMAP_TABLE BitmapTable;
    PDICTIONARY_CONTEXT Context;
    PHISTOGRAM_TABLE HistogramTable;
    PRTL_LOOKUP_ELEMENT_GENERIC_TABLE_AVL RtlLookupElementGenericTableAvl;

    PWORD_TABLE_ENTRY WordTableEntry;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;

    TABLE_ENTRY_HEADER WordTableEntryHeader;
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

    if (!ARGUMENT_PRESENT(WordTableEntryPointer)) {
        return FALSE;
    }

    //
    // Clear the caller's word table entry pointer up-front.
    //

    *WordTableEntryPointer = NULL;

    //
    // Get the TLS context.
    //

    Context = DictionaryTlsGetContext();

    //
    // Zero local structures.
    //

    ZeroStruct(WordTableEntryHeader);
    ZeroStruct(BitmapTableEntryHeader);
    ZeroStruct(HistogramTableEntryHeader);

    //
    // Initialize aliases and various table entry pointers.
    //

    Rtl = Dictionary->Rtl;
    RtlLookupElementGenericTableAvl = Rtl->RtlLookupElementGenericTableAvl;

    WordTableEntry = HEADER_TO_WORD_TABLE_ENTRY(&WordTableEntryHeader);
    BitmapTableEntry = HEADER_TO_BITMAP_TABLE_ENTRY(&BitmapTableEntryHeader);
    HistogramTableEntry =
        HEADER_TO_HISTOGRAM_TABLE_ENTRY(&HistogramTableEntryHeader);

    String = &WordTableEntry->WordEntry.String;
    BitmapHash = &BitmapTableEntryHeader.Hash;
    HistogramHash = &HistogramTableEntryHeader.Hash;

    //
    // Initialize the word.  This will verify the length of the incoming string
    // as well as calculate the bitmap and histogram and respective hashes.  We
    // then use these as part of the AVL table lookups.
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
    // Copy the word's hash into the appropriate location within the word
    // table entry's header.
    //

    WordTableEntryHeader.Hash = String->Hash;

    //
    // Lookup the bitmap.
    //

    BitmapTable = &Dictionary->BitmapTable;
    BitmapTableEntry = RtlLookupElementGenericTableAvl(&BitmapTable->Avl,
                                                       BitmapTableEntry);

    if (!BitmapTableEntry) {

        //
        // No bitmap table entry match.
        //

        goto End;
    }

    //
    // Update the TLS context and initialize the histogram table alias, then
    // search for the histogram.
    //

    Context->BitmapTableEntry = BitmapTableEntry;
    HistogramTable = &BitmapTableEntry->HistogramTable;

    HistogramTableEntry = RtlLookupElementGenericTableAvl(&HistogramTable->Avl,
                                                          HistogramTableEntry);

    if (!HistogramTableEntry) {

        //
        // No histogram table entry match.
        //

        goto End;

    }

    //
    // Update the TLS context and initialize the word entry alias.
    //

    Context->HistogramTableEntry = HistogramTableEntry;
    WordTable = &HistogramTableEntry->WordTable;

    WordTableEntry = RtlLookupElementGenericTableAvl(&WordTable->Avl,
                                                     WordTableEntry);

    if (!WordTableEntry) {

        //
        // No word table entry match.
        //

        goto End;
    }

    //
    // We found the word!  Update the TLS context and the caller's pointer.
    //

    Context->WordTable = WordTable;
    Context->WordTableEntry = WordTableEntry;

    *WordTableEntryPointer = WordTableEntry;

    //
    // Intentional follow-on to End.
    //

End:

    Success = TRUE;

    return Success;
}

_Use_decl_annotations_
BOOLEAN
FindWord(
    PDICTIONARY Dictionary,
    PCBYTE Word,
    PBOOLEAN Exists
    )
/*++

Routine Description:

    TBD.

Arguments:

    TBD.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BOOL Success;
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

    if (!ARGUMENT_PRESENT(Exists)) {
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

        //
        // No match found.
        //

        *Exists = FALSE;

    } else {

        //
        // Match found!
        //

        *Exists = TRUE;

    }

    ReleaseDictionaryLockShared(&Dictionary->Lock);

    return TRUE;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
