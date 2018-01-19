/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Dictionary.c

Abstract:

    This is the main header file for the Dictionary component.

--*/

#include "stdafx.h"

_Use_decl_annotations_
BOOLEAN
CreateAndInitializeDictionary(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY *DictionaryPointer
    )
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
    // Clear the caller's pointer up-front.
    //

    *DictionaryPointer = NULL;

    //
    // Allocate space for the dictionary structure.
    //

    Dictionary = (PDICTIONARY)Allocator->Calloc(Allocator, 1, sizeof(*Dictionary));

    if (!Dictionary) {
        goto Error;
    }

    //
    // Allocation was successful, continue with initialization.
    //

    Dictionary->SizeOfStruct = sizeof(*Dictionary);
    Dictionary->Rtl = Rtl;

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

    *DictionaryPointer = Dictionary;

    return Success;
}

//
// Exported versions of inline routines provided in the public header file.
//

_Use_decl_annotations_
BOOLEAN
CreateCharacterBitmapForString(
    PCLONG_STRING String,
    PCHARACTER_BITMAP Bitmap
    )
{
    return CreateCharacterBitmapForStringInline(String, Bitmap);
}

_Use_decl_annotations_
BOOLEAN
CreateCharacterHistogramForStringHash32(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM Histogram,
    PULONG Hash32Pointer
    )
{
    return CreateCharacterHistogramForStringHash32Inline(String,
                                                         Histogram,
                                                         Hash32Pointer);
}

_Use_decl_annotations_
BOOLEAN
CreateCharacterHistogramForStringHash64(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM Histogram,
    PULONGLONG Hash64Pointer
    )
{
    return CreateCharacterHistogramForStringHash64Inline(String,
                                                         Histogram,
                                                         Hash64Pointer);
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
