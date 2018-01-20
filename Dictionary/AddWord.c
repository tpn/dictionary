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
    PCLONG_STRING Word,
    PCWORD_ENTRY *WordEntryPointer,
    PULONGLONG EntryCount
)
/*++

Routine Description:

    Adds a word to a dictionary if it doesn't exist, increments the word's
    existing count if it does.

Arguments:

    TBD.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BOOL Success;
    CHARACTER_BITMAP Bitmap;
    CHARACTER_HISTOGRAM Histogram;
    ULONG HistogramHash;
    BOOLEAN NewEntry;
    BITMAP_TABLE_ENTRY BitmapTableEntry;
    BITMAP_TABLE_ENTRY_FULL BitmapTableEntryFull;

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
    // Convert the incoming string into bitmap and histogram
    // representations.
    //

    Success = CreateCharacterBitmapForString(Word, &Bitmap);
    if (!Success) {
        goto Error;
    }

    Success = CreateCharacterHistogramForStringHash32(
        Word,
        &Histogram,
        &HistogramHash
    );

    if (!Success) {
        goto Error;
    }

    //
    // We've captured the bitmap and histogram primary keys used
    // to identify the incoming word, plus a simple 32-bit hash
    // value that we can use to perform quick identity checks on
    // two incoming words as part of determining whether or not
    // they're identical.
    //
    
    ZeroStruct(BitmapTableEntryFull);

    Entry->Hash = BitmapHash;
    

    Table = &Dictionary->BitmapTable.Avl;
    
    AcquireDictionaryLockExclusive(&Dictionary->Lock);
    
    BitmapTableEntry = Rtl->RtlInsertGenericTableAvl(
        Table,
        &BitmapTableEntry,
        sizeof(BITMAP_TABLE_ENTRY),
        &NewElement
    );

    ReleaseDictionaryLockExclusive(&Dictionary->Lock);

    if (!BitmapTableEntry) {
        __debugbreak();
        goto Error;
    }

    //
    // Existing entry matching this histogram hash has been found,
    // or a new entry was created.  This will be indicated by the
    // NewElement parameter.
    //

    if (NewElement) {

        //
        // Need to allocate an initialize the histogram table to
        // be used as the second level lookup.
        //

        NOTHING;

    } else {
        
        //
        // Presume we'll need to do something useful here.
        //

        NOTHING;
    }



 Error:
    
    Success = FALSE;

    //
    // Intentional follow-on to End.
    //

End:

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
