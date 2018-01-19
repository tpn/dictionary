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
CreateAndInitializeDictionary(
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

    //
    // Initialize the dictionary lock, acquire it exclusively, then initialize
    // the underlying AVL table.  (We acquire and release it to satisfy the SAL
    // _Guarded_by_(Lock) annotation on the underlying BitmapTable element.)
    //

    InitializeDictionaryLock(&Dictionary->Lock);

    AcquireDictionaryLockExclusive(&Dictionary->Lock);

    Rtl->RtlInitializeGenericTableAvl(&Dictionary->BitmapTable,
                                      BitmapTableCompareRoutine,
                                      BitmapTableAllocateRoutine,
                                      BitmapTableFreeRoutine,
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
