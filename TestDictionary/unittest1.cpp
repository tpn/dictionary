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

#define MAKE_BYTES(Value) ((PCBYTE)Value)

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
MAKE_STRING(QuickFox, "The quick brown fox jumps over the lazy dog.");
MAKE_STRING(LazyDog,  "The lazy dog jumps over the quick brown fox.");

RTL GlobalRtl;
ALLOCATOR GlobalAllocator;

RTL_BOOTSTRAP GlobalBootstrap;
PRTL_BOOTSTRAP Bootstrap;

PRTL Rtl;
PALLOCATOR Allocator;

DICTIONARY_FUNCTIONS GlobalApi;
PDICTIONARY_FUNCTIONS Api;

HMODULE GlobalRtlModule = 0;
HMODULE GlobalDictionaryModule = 0;

TEST_MODULE_INITIALIZE(UnitTest1Init)
{
    ULONG SizeOfRtl = sizeof(GlobalRtl);

    Assert::IsTrue(BootstrapRtl(&GlobalRtlModule, &GlobalBootstrap));

    Bootstrap = &GlobalBootstrap;

    Assert::IsTrue(Bootstrap->InitializeRtl(&GlobalRtl, &SizeOfRtl));
    Assert::IsTrue(Bootstrap->InitializeHeapAllocator(&GlobalAllocator));

    Rtl = &GlobalRtl;
    Allocator = &GlobalAllocator;

    Assert::IsTrue(LoadDictionaryModule(Rtl,
                                        &GlobalDictionaryModule,
                                        &GlobalApi));

    Api = &GlobalApi;
}

TEST_MODULE_CLEANUP(UnitTest1Cleanup)
{
    if (GlobalRtlModule != 0) {
        FreeLibrary(GlobalRtlModule);
    }

    if (GlobalDictionaryModule != 0) {
        FreeLibrary(GlobalDictionaryModule);
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
            PCWORD_ENTRY WordEntry;
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
                             Elbow.Buffer,
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
            PCWORD_ENTRY WordEntry;
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
                             Elbow.Buffer,
                             &WordEntry,
                             &EntryCount)
            );

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             Elbow.Buffer,
                             &WordEntry,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        //
        // Verify trailing bytes after the NUL terminator are ignored.
        //

        TEST_METHOD(AddWordNullTerminatorVerification1)
        {
            BYTE Index;
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            PCWORD_ENTRY WordEntry;
            LONGLONG EntryCount;
            PCBYTE Strings[] = {
                (PCBYTE)"a\0b",
                (PCBYTE)"a\0bc",
                (PCBYTE)"a\0bcd",
                (PCBYTE)"a\0bcde",
            };
            PCBYTE String;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            for (Index = 0; Index < ARRAYSIZE(Strings); Index++) {
                String = Strings[Index];

                Assert::IsTrue(
                    Api->AddWord(Dictionary,
                                 String,
                                 &WordEntry,
                                 &EntryCount)
                );

                Assert::IsTrue(EntryCount == (LONGLONG)(Index+1));
            }

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(AddWordNullTerminatorVerification2)
        {
            BYTE Index;
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            PCWORD_ENTRY WordEntry;
            LONGLONG EntryCount;
            PCBYTE Strings[] = {
                (PCBYTE)"abcd\0e",
                (PCBYTE)"abcd\0ef",
                (PCBYTE)"abcd\0efg",
                (PCBYTE)"abcd\0efgh",
            };
            PCBYTE String;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            for (Index = 0; Index < ARRAYSIZE(Strings); Index++) {
                String = Strings[Index];

                Assert::IsTrue(
                    Api->AddWord(Dictionary,
                                 String,
                                 &WordEntry,
                                 &EntryCount)
                );

                Assert::IsTrue(EntryCount == (LONGLONG)(Index+1));
            }

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(AddWordNullTerminatorVerification3)
        {
            BYTE Index;
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            PCWORD_ENTRY WordEntry;
            LONGLONG EntryCount;
            PCBYTE Strings[] = {
                (PCBYTE)"abcd1\0e",
                (PCBYTE)"abcd1\0ef",
                (PCBYTE)"abcd1\0efg",
                (PCBYTE)"abcd1\0efgh",
            };
            PCBYTE String;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            for (Index = 0; Index < ARRAYSIZE(Strings); Index++) {
                String = Strings[Index];

                Assert::IsTrue(
                    Api->AddWord(Dictionary,
                                 String,
                                 &WordEntry,
                                 &EntryCount)
                );

                Assert::IsTrue(EntryCount == (LONGLONG)(Index+1));
            }

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
