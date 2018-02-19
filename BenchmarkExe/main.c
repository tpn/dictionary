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

BOOLEAN
MakeRandomString(
    PRTL Rtl,
    PALLOCATOR Allocator,
    ULONG BufferSize,
    PBYTE *BufferPointer
    )
{
    BOOLEAN Success;
    PBYTE Buffer;
    ULONGLONG BytesReplaced;

    *BufferPointer = NULL;

    Buffer = (PBYTE)Allocator->Malloc(Allocator, BufferSize);
    if (!Buffer) {
        return FALSE;
    }

    if (!Rtl->CryptGenRandom(Rtl, BufferSize, Buffer)) {
        goto Error;
    }

    Buffer[2] = 0x0;
    Buffer[57] = 0x0;

    BytesReplaced = Rtl->FindAndReplaceByte(BufferSize,
                                            Buffer,
                                            0x0,
                                            0xcc);

    if (BytesReplaced < 2) {
        __debugbreak();
        goto Error;
    }

    Success = TRUE;
    goto End;

Error:

    Success = FALSE;
    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    //
    // Intentional follow-on to End.
    //

End:

    *BufferPointer = Buffer;

    return Success;
}


VOID
Scratch3(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    PBYTE Buffer;
    ULONG BufferSize = 1 << 16; // 64 KB
    LONG_STRING String;
    BOOLEAN Result1;
    BOOLEAN Result2;
    CHARACTER_HISTOGRAM HistogramA;
    CHARACTER_HISTOGRAM_V4 HistogramB;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;
    RTL_GENERIC_COMPARE_RESULTS Comparison;
    LARGE_INTEGER Frequency;
    LARGE_INTEGER Start1;
    LARGE_INTEGER Start2;
    LARGE_INTEGER End1;
    LARGE_INTEGER End2;
    LARGE_INTEGER Elapsed1;
    LARGE_INTEGER Elapsed2;

    ZeroStruct(HistogramA);
    ZeroStruct(HistogramB);

    ASSERT(
        MakeRandomString(Rtl,
                         Allocator,
                         BufferSize,
                         &Buffer)
    );

    String.Length = BufferSize;
    String.Hash = 0;
    String.Buffer = Buffer;

    QueryPerformanceFrequency(&Frequency);

    QueryPerformanceCounter(&Start1);
    Result1 = Api->CreateHistogram(&String, &HistogramA);
    QueryPerformanceCounter(&End1);

    Elapsed1.QuadPart = End1.QuadPart - Start1.QuadPart;

    ASSERT(Result1);

    QueryPerformanceCounter(&Start2);
    Result2 = Api->CreateHistogramAvx2C(&String,
                                        &HistogramB.Histogram1,
                                        &HistogramB.Histogram2);
    QueryPerformanceCounter(&End2);

    Elapsed2.QuadPart = End2.QuadPart - Start2.QuadPart;

    ASSERT(Result2);

    Histogram1 = &HistogramA;
    Histogram2 = &HistogramB.Histogram1;

    Comparison = Api->CompareHistograms(Histogram1, Histogram2);

    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    //ASSERT(Elapsed2.QuadPart < Elapsed1.QuadPart);
    ASSERT(Comparison == GenericEqual);

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

    //Scratch2(Rtl, Allocator, Api);

    Scratch3(Rtl, Allocator, Api);

Error:

    ExitProcess(ExitCode);
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
