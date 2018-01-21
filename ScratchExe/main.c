/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    main.c

Abstract:

    Scratch/testing file.

--*/

#include "stdafx.h"
#include "../Dictionary/Dictionary.h"

RTL GlobalRtl;
ALLOCATOR GlobalAllocator;

PRTL Rtl;
PALLOCATOR Allocator;

DICTIONARY_FUNCTIONS GlobalApi;
PDICTIONARY_FUNCTIONS Api;

HMODULE GlobalModule = 0;

VOID
Scratch2(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    BOOLEAN Success;
    DICTIONARY_CREATE_FLAGS CreateFlags;
    PDICTIONARY Dictionary;
    BOOLEAN IsProcessTerminating;
    PWORD_ENTRY WordEntry;
    LONGLONG EntryCount;

    CreateFlags.AsULong = 0;
    IsProcessTerminating = FALSE;

    Success = Api->CreateDictionary(Rtl,
                                    Allocator,
                                    CreateFlags,
                                    &Dictionary);

    if (!Success) {
        __debugbreak();
    }

    Success = Api->AddWord(Dictionary,
                           "elbow",
                           &WordEntry,
                           &EntryCount);

    if (!Success) {
        __debugbreak();
    }

    Success = Api->DestroyDictionary(&Dictionary, &IsProcessTerminating);

    if (!Success) {
        __debugbreak();
    }

    return;
}

DECLSPEC_NORETURN
VOID
WINAPI
mainCRTStartup()
{
    LONG ExitCode = 0;
    LONG SizeOfRtl = sizeof(GlobalRtl);
    HMODULE RtlModule = LoadLibraryA("Rtl.dll");
    PROC Proc = GetProcAddress(RtlModule, "InitializeRtl");
    PINITIALIZE_RTL InitializeRtl = (PINITIALIZE_RTL)Proc;

    //
    // Initialization glue for our allocator, config, rtl and dictionary.
    //

    if (!DefaultHeapInitializeAllocator(&GlobalAllocator)) {
        ExitCode = 1;
        goto Error;
    }

    CHECKED_MSG(
        InitializeRtl(&GlobalRtl, &SizeOfRtl),
        "InitializeRtl()"
    );

    Rtl = &GlobalRtl;
    Allocator = &GlobalAllocator;

    CHECKED_MSG(
        LoadDictionaryModule(
            Rtl,
            &GlobalModule,
            &GlobalApi
        ),
        "LoadDictionaryModule"
    );

    Api = &GlobalApi;

    // Scratch1();

    Scratch2(Rtl, Allocator, Api);

Error:

    ExitProcess(ExitCode);
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
