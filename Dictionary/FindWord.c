/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    FindWord.c

Abstract:

    This module implements the find word functionality for the dictionary
    component.  Two routines a provided: FindWord, which is the publicly
    exposed API function, and FindWordTableEntry, which is private to the
    dictionary module.  The latter is also used by the GetWordAnagrams and
    RemoveWord routines, and allows the caller to capture the bitmap and
    histogram representations of a word, as well as the corresponding word
    table entry.

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

    Finds the word table entry for a given word in a dictionary.  This is
    a private routine that is used by both FindWord and RemoveWord, hence
    its use of the private type WORD_TABLE_ENTRY as an output parameter.

Arguments:

    Dictionary - Supplies a pointer to a DICTIONARY structure for which the
        given word is to be found.

    Word - Supplies a NULL-terminated array of bytes representing the word to
        find in the dictionary.

    Bitmap - Supplies a pointer to a CHARACTER_BITMAP structure that will
        receive the corresponding bitmap representation of the incoming word.
        (This parameter is passed directly to InitializeWord.)

    Histogram - Supplies a pointer to a CHARACTER_HISTOGRAM structure that
        will receive the corresponding histogram representation of the incoming
        word.  (This parameter is passed directly to InitializeWord.)

    WordEntryPointer - Supplies an address to a variable that receives the
        address of the WORD_ENTRY structure representing the word found if
        no error occurred.  Will be set to NULL on error.

Return Value:

    TRUE on success, FALSE on failure.  If no word is found, TRUE will be
    returned and the caller's WordTableEntryPointer output parameter will
    be set to NULL.

    If TRUE is returned, both the Bitmap and Histogram will be filled out.

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

    WordTableEntry = &WordTableEntryHeader.WordTableEntry;
    BitmapTableEntry = &BitmapTableEntryHeader.BitmapTableEntry;
    HistogramTableEntry = &HistogramTableEntryHeader.HistogramTableEntry;

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
    Context->HistogramTable = HistogramTable;

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

    This is the public routine for determining whether or not a word exists
    in a dictionary.

Arguments:

    Dictionary - Supplies a pointer to a DICTIONARY structure for which the
        given word is to be found.

    Word - Supplies a NULL-terminated array of bytes representing the word to
        find in the dictionary.

    Exists - Supplies the address of a variable that will receive a boolean
        flag indicating whether or not the word was found in the dictionary.

Return Value:

    TRUE on success, FALSE on failure.  If no word is found, TRUE will be
    returned and FALSE will be written to the Exists output parameter.

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

    //
    // Release the lock and indicate success.
    //

    ReleaseDictionaryLockShared(&Dictionary->Lock);

    return TRUE;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
