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