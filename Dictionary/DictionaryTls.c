/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    DictionaryTls.c

Abstract:

    This module provides TLS glue to facilitate access to the current
    dictionary context whilst within AVL table routine callbacks.  This
    is done to avoid the extra pointer overhead that would be required for
    nodes to point back to their owning parent dictionary.

--*/

#include "stdafx.h"

//
// Our TLS index.  Assigned at PROCESS_ATTACH, free'd at PROCESS_DETACH.
//

ULONG DictionaryTlsIndex;

DICTIONARY_TLS_FUNCTION DictionaryTlsProcessAttach;

_Use_decl_annotations_
DictionaryTlsProcessAttach(
    HMODULE Module,
    ULONG   Reason,
    LPVOID  Reserved
    )
{
    DictionaryTlsIndex = TlsAlloc();

    if (DictionaryTlsIndex == TLS_OUT_OF_INDEXES) {
        return FALSE;
    }

    return TRUE;
}

DICTIONARY_TLS_FUNCTION DictionaryTlsProcessDetach;

_Use_decl_annotations_
DictionaryTlsProcessDetach(
    HMODULE Module,
    ULONG   Reason,
    LPVOID  Reserved
    )
{
    BOOL IsProcessTerminating;

    IsProcessTerminating = (Reserved != NULL);

    if (IsProcessTerminating) {
        goto End;
    }

    if (DictionaryTlsIndex == TLS_OUT_OF_INDEXES) {
        goto End;
    }

    if (!TlsFree(DictionaryTlsIndex)) {

        //
        // Can't do anything here.
        //

        NOTHING;
    }

    //
    // Note that we always return TRUE here, even if we had a failure.  We're
    // only called at DLL_PROCESS_DETACH, so there's not much we can do when
    // there is actually an error anyway.
    //

End:

    return TRUE;
}

//
// TLS Set/Get Context functions.
//

DICTIONARY_TLS_SET_CONTEXT DictionaryTlsSetContext;

_Use_decl_annotations_
BOOLEAN
DictionaryTlsSetContext(
    PDICTIONARY_CONTEXT Context
    )
{
    return TlsSetValue(DictionaryTlsIndex, Context);
}

DICTIONARY_TLS_GET_CONTEXT DictionaryTlsGetContext;

_Use_decl_annotations_
PDICTIONARY_CONTEXT
DictionaryTlsGetContext(
    VOID
    )
{
    PVOID Value;

    Value = TlsGetValue(DictionaryTlsIndex);

    return (PDICTIONARY_CONTEXT)Value;
}


// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
