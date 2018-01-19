/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    main.c

Abstract:

    Scratch/testing file.

--*/

#include "stdafx.h"
#include "../Dictionary/Dictionary.h"

VOID
Scratch1(
    PRTL Rtl,
    PDICTIONARY Dictionary
    )
{
    BOOLEAN Success;
    ULONG FooHistogramHash;
    ULONG OofHistogramHash;
    CHARACTER_BITMAP FooBitmap;
    CHARACTER_BITMAP OofBitmap;
    CHARACTER_HISTOGRAM FooHistogram;
    CHARACTER_HISTOGRAM OofHistogram;
    LONG_STRING Foo = CONSTANT_LONG_STRING("foo");
    LONG_STRING Oof = CONSTANT_LONG_STRING("oof");

    Success = CreateCharacterBitmapForStringInline(&Foo, &FooBitmap);
    if (!Success) {
        __debugbreak();
        return;
    }

    Success = CreateCharacterBitmapForStringInline(&Oof, &OofBitmap);
    if (!Success) {
        __debugbreak();
        return;
    }

    if (FooBitmap.Hash != OofBitmap.Hash) {
        __debugbreak();
        return;
    }

    Success =
        CreateCharacterHistogramForStringHash32Inline(
            &Foo,
            &FooHistogram,
            &FooHistogramHash
            );

    if (!Success) {
        __debugbreak();
    }

    Success =
        CreateCharacterHistogramForStringHash32Inline(
            &Oof,
            &OofHistogram,
            &OofHistogramHash
            );

    if (!Success) {
        __debugbreak();
    }

    if (FooHistogramHash != OofHistogramHash) {
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

    PRTL Rtl;
    BOOL Success;
    PTRACER_CONFIG TracerConfig;
    ALLOCATOR Allocator;
    PDICTIONARY Dictionary;
    DICTIONARY_CREATE_FLAGS CreateFlags;

    //
    // Initialization glue for our allocator, config, rtl and dictionary.
    //

    if (!DefaultHeapInitializeAllocator(&Allocator)) {
        ExitCode = 1;
        goto Error;
    }

    CHECKED_MSG(
        CreateAndInitializeTracerConfigAndRtl(
            &Allocator,
            (PUNICODE_STRING)&TracerRegistryPath,
            &TracerConfig,
            &Rtl
        ),
        "CreateAndInitializeTracerConfigAndRtl()"
    );

    CreateFlags.AsULong = 0;

    Success = CreateAndInitializeDictionary(Rtl,
                                            &Allocator,
                                            CreateFlags,
                                            &Dictionary); 

    if (!Success) {
        ExitCode = 1;
        goto Error;
    }

    Scratch1(Rtl, Dictionary);

Error:

    ExitProcess(ExitCode);
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
