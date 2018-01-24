/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    main.c

Abstract:

    Scratch/testing file.

--*/

#include "stdafx.h"

RTL GlobalRtl;
ALLOCATOR GlobalAllocator;

PRTL Rtl;
PALLOCATOR Allocator;

DICTIONARY_FUNCTIONS GlobalApi;
PDICTIONARY_FUNCTIONS Api;

HMODULE GlobalModule = 0;

#ifndef ASSERT
#define ASSERT(Condition)   \
    if (!(Condition)) {     \
        __debugbreak();     \
    }
#endif

VOID
Scratch2(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    DICTIONARY_CREATE_FLAGS CreateFlags;
    PDICTIONARY Dictionary;
    BOOLEAN IsProcessTerminating;
    LONGLONG EntryCount;
    PDICTIONARY_STATS Stats;

    CreateFlags.AsULong = 0;
    IsProcessTerminating = FALSE;

    ASSERT(Api->CreateDictionary(Rtl,
                                    Allocator,
                                    CreateFlags,
                                    &Dictionary));

    ASSERT(Api->AddWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->GetDictionaryStats(Dictionary,
                                      Allocator,
                                      &Stats));

    ASSERT(Api->AddWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->RemoveWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "below",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "below",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "The quick brown fox jumped over the lazy dog.",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "The quick brown fox jumped over the lazy dog.",
                           &EntryCount));

    ASSERT(Api->DestroyDictionary(&Dictionary, &IsProcessTerminating));

    return;
}

DECLSPEC_NORETURN
VOID
WINAPI
mainCRTStartup()
{
    LONG ExitCode = 0;
    LONG SizeOfRtl = sizeof(GlobalRtl);
    HMODULE RtlModule;
    RTL_BOOTSTRAP Bootstrap;

    if (!BootstrapRtl(&RtlModule, &Bootstrap)) {
        ExitCode = 1;
        goto Error;
    }

    if (!Bootstrap.InitializeHeapAllocator(&GlobalAllocator)) {
        ExitCode = 1;
        goto Error;
    }

    CHECKED_MSG(
        Bootstrap.InitializeRtl(&GlobalRtl, &SizeOfRtl),
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
