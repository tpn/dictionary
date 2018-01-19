/*++

Copyright (c) 2016 Trent Nelson <trent@trent.me>

Module Name:

    DictionaryPrivate.h

Abstract:

    This is the private header file for the Dictionary component.  It defines
    function typedefs and function declarations for all major (i.e. not local
    to the module) functions available for use by individual modules within
    this component.

--*/

#ifndef _DICTIONARY_INTERNAL_BUILD
#error DictionaryPrivate.h being included but _DICTIONARY_INTERNAL_BUILD not set.
#endif

#pragma once

#include "stdafx.h"

//
// Define the DICTIONARY structure.
//

typedef union _DICTIONARY_FLAGS {
    struct _Struct_size_bytes_(sizeof(ULONG)) {

        //
        // Unused bits.
        //

        ULONG Unused:32;
    };

    LONG AsLong;
    ULONG AsULong;
} DICTIONARY_FLAGS;

typedef struct _Struct_size_bytes_(SizeOfStruct) _DICTIONARY {

    //
    // Size of the structure, in bytes.
    //

    _Field_range_(==, sizeof(struct _DICTIONARY)) ULONG SizeOfStruct;

    //
    // Dictionary flags.
    //

    DICTIONARY_FLAGS Flags;

    //
    // Pointer to an initialized RTL structure.
    //

    PRTL Rtl;

} DICTIONARY;
typedef DICTIONARY *PDICTIONARY;


// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
