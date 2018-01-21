/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    unittest1.cpp

Abstract:

    This module is the first set of unit tests for the Dictionary component.
    It uses C++ instead of C simply to leverage the C++ test framework provided
    with Visual Studio.

--*/

#include "stdafx.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

//
// Define sample string macros and constants to help with testing.
//

#define MAKE_STRING(Name, Value)                                       \
    static const LONG_STRING Name = CONSTANT_LONG_STRING((PBYTE)Value)

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(Name) ((VOID)Name)
#endif

#define STRING_DECL(Name)                    \
    PCLONG_STRING Name##String = &##Name;    \
    ULONG Name##HistogramHash32;             \
    ULONGLONG Name##HistogramHash64;         \
    CHARACTER_BITMAP Name##Bitmap;           \
    CHARACTER_HISTOGRAM Name##Histogram;     \
    UNUSED_PARAMETER(Name##Bitmap);          \
    UNUSED_PARAMETER(Name##Histogram);       \
    UNUSED_PARAMETER(Name##HistogramHash32); \
    UNUSED_PARAMETER(Name##HistogramHash64)

#define MAKE_BITMAP(Name)               \
    Assert::IsTrue(                     \
        CreateCharacterBitmapForString( \
            Name##String,               \
            &Name##Bitmap               \
        )                               \
    )

#define MAKE_HISTOGRAM_HASH32(Name)              \
    Assert::IsTrue(                              \
        CreateCharacterHistogramForStringHash32( \
            Name##String,                        \
            &Name##Histogram,                    \
            &##Name##HistogramHash32             \
        )                                        \
    )

#define MAKE_HISTOGRAM_HASH64(Name)              \
    Assert::IsTrue(                              \
        CreateCharacterHistogramForStringHash64( \
            Name##String,                        \
            &Name##Histogram,                    \
            &##Name##HistogramHash64             \
        )                                        \
    )

MAKE_STRING(Below, "below");
MAKE_STRING(Elbow, "elbow");

RTL GlobalRtl;
ALLOCATOR GlobalAllocator;

PRTL Rtl;
PALLOCATOR Allocator;

DICTIONARY_FUNCTIONS GlobalApi;
PDICTIONARY_FUNCTIONS Api;

HMODULE GlobalModule = 0;

TEST_MODULE_INITIALIZE(UnitTest1Init)
{
    ULONG SizeOfRtl = sizeof(GlobalRtl);

    Assert::IsTrue(InitializeRtl(&GlobalRtl, &SizeOfRtl));
    Assert::IsTrue(DefaultHeapInitializeAllocator(&GlobalAllocator));

    Rtl = &GlobalRtl;
    Allocator = &GlobalAllocator;

    Assert::IsTrue(LoadDictionaryModule(Rtl,
                                        &GlobalModule,
                                        &GlobalApi));

    Api = &GlobalApi;
}

TEST_MODULE_CLEANUP(UnitTest1Cleanup)
{
    if (GlobalModule != 0) {
        FreeLibrary(GlobalModule);
    }
}

namespace TestDictionary
{
    TEST_CLASS(UnitTest1)
    {
    public:

        TEST_METHOD(CreateAndDestroy1)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(AddWord1)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            PWORD_ENTRY WordEntry;
            LONGLONG EntryCount;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             "elbow",
                             &WordEntry,
                             &EntryCount)
            );

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(AddWordDuplicate1)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            PWORD_ENTRY WordEntry;
            LONGLONG EntryCount;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             "elbow",
                             &WordEntry,
                             &EntryCount)
            );

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             "elbow",
                             &WordEntry,
                             &EntryCount)
            );

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

    };
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
