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
FindWord(
    PDICTIONARY Dictionary,
    PCLONG_STRING Word,
    PCWORD_ENTRY *WordEntryPointer
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

    Success = FALSE;

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
