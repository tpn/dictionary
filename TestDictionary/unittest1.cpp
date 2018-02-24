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

#define MAKE_STRING(Name, Value)                                               \
    static const LONG_STRING Name##String = CONSTANT_LONG_STRING(Value);       \
    static PCBYTE Name = Name##String.Buffer;                                  \
    static ULONG Name##Length = Name##String.Length

MAKE_STRING(Below, "below");
MAKE_STRING(Elbow, "elbow");
MAKE_STRING(QuickFox,  "The quick brown fox jumps over the lazy dog.");
MAKE_STRING(LazyDog,   "The lazy dog jumps over the quick brown fox.");
MAKE_STRING(QuickLazy, "The quick brown fox jumps over the lazy dog and then "
                       "the lazy dog jumps over the quick brown fox.");

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
                             Elbow,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(AddWord2)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;
            PBYTE RandomBuffer;
            ULONG BufferSize = 1 << 16; // 64 KB
            ULONGLONG BytesReplaced;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            RandomBuffer = (PBYTE)Allocator->Malloc(Allocator, BufferSize);
            Assert::IsTrue(RandomBuffer != NULL);

            Assert::IsTrue(Rtl->CryptGenRandom(Rtl, BufferSize, RandomBuffer));

            RandomBuffer[2] = 0x0;
            RandomBuffer[57] = 0x0;

            BytesReplaced = Rtl->FindAndReplaceByte(BufferSize,
                                                    RandomBuffer,
                                                    0x0,
                                                    0xcc);

            Assert::IsTrue(BytesReplaced >= 2);

            //
            // NULL terminate the buffer.
            //

            RandomBuffer[BufferSize-1] = 0x0;

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             RandomBuffer,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(CreateHistogram1ShortestString)
        {
            BOOLEAN Result1;
            BOOLEAN Result2;
            PCLONG_STRING String;
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

            String = &ElbowString;

            QueryPerformanceFrequency(&Frequency);

            QueryPerformanceCounter(&Start1);
            Result1 = CreateHistogramInline(String, &HistogramA);
            QueryPerformanceCounter(&End1);

            Elapsed1.QuadPart = End1.QuadPart - Start1.QuadPart;

            Assert::IsTrue(Result1);

            QueryPerformanceCounter(&Start2);
            Result2 = CreateHistogramAvx2Inline(String,
                                                &HistogramB.Histogram1,
                                                &HistogramB.Histogram2);
            QueryPerformanceCounter(&End2);

            Elapsed2.QuadPart = End2.QuadPart - Start2.QuadPart;

            Assert::IsTrue(Result2);

            Histogram1 = &HistogramA;
            Histogram2 = &HistogramB.Histogram1;

            Comparison = CompareHistogramsAlignedAvx2Inline(Histogram1,
                                                            Histogram2);


            //Assert::IsTrue(Elapsed2.QuadPart < Elapsed1.QuadPart);
            Assert::IsTrue(Comparison == GenericEqual);

        }

        TEST_METHOD(CreateHistogram1StringLongerThan32Bytes)
        {
            BOOLEAN Result1;
            BOOLEAN Result2;
            PCLONG_STRING String;
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

            String = &QuickFoxString;

            QueryPerformanceFrequency(&Frequency);

            QueryPerformanceCounter(&Start1);
            Result1 = CreateHistogramInline(String, &HistogramA);
            QueryPerformanceCounter(&End1);

            Elapsed1.QuadPart = End1.QuadPart - Start1.QuadPart;

            Assert::IsTrue(Result1);

            QueryPerformanceCounter(&Start2);
            Result2 = CreateHistogramAvx2Inline(String,
                                                &HistogramB.Histogram1,
                                                &HistogramB.Histogram2);
            QueryPerformanceCounter(&End2);

            Elapsed2.QuadPart = End2.QuadPart - Start2.QuadPart;

            Assert::IsTrue(Result2);

            Histogram1 = &HistogramA;
            Histogram2 = &HistogramB.Histogram1;

            Comparison = CompareHistogramsAlignedAvx2Inline(Histogram1,
                                                            Histogram2);


            //Assert::IsTrue(Elapsed2.QuadPart < Elapsed1.QuadPart);
            Assert::IsTrue(Comparison == GenericEqual);

        }

        TEST_METHOD(CreateHistogram1StringLongerThan64Bytes)
        {
            BOOLEAN Result1;
            BOOLEAN Result2;
            PCLONG_STRING String;
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

            String = &QuickLazyString;

            QueryPerformanceFrequency(&Frequency);

            QueryPerformanceCounter(&Start1);
            Result1 = CreateHistogramInline(String, &HistogramA);
            QueryPerformanceCounter(&End1);

            Elapsed1.QuadPart = End1.QuadPart - Start1.QuadPart;

            Assert::IsTrue(Result1);

            QueryPerformanceCounter(&Start2);
            Result2 = CreateHistogramAvx2Inline(String,
                                                &HistogramB.Histogram1,
                                                &HistogramB.Histogram2);
            QueryPerformanceCounter(&End2);

            Elapsed2.QuadPart = End2.QuadPart - Start2.QuadPart;

            Assert::IsTrue(Result2);

            Histogram1 = &HistogramA;
            Histogram2 = &HistogramB.Histogram1;

            Comparison = CompareHistogramsAlignedAvx2Inline(Histogram1,
                                                            Histogram2);


            //Assert::IsTrue(Elapsed2.QuadPart > Elapsed1.QuadPart);
            Assert::IsTrue(Comparison == GenericEqual);

        }


        TEST_METHOD(CreateHistogram3LongString)
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

            Assert::IsTrue(
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
            Result1 = CreateHistogramInline(&String, &HistogramA);
            QueryPerformanceCounter(&End1);

            Elapsed1.QuadPart = End1.QuadPart - Start1.QuadPart;

            Assert::IsTrue(Result1);

            QueryPerformanceCounter(&Start2);
            Result2 = CreateHistogramAvx2Inline(&String,
                                                &HistogramB.Histogram1,
                                                &HistogramB.Histogram2);
            QueryPerformanceCounter(&End2);

            Elapsed2.QuadPart = End2.QuadPart - Start2.QuadPart;

            Assert::IsTrue(Result2);

            Histogram1 = &HistogramA;
            Histogram2 = &HistogramB.Histogram1;

            Comparison = CompareHistogramsAlignedAvx2Inline(Histogram1,
                                                            Histogram2);


            Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

            Assert::IsTrue(Comparison == GenericEqual);
        }

        TEST_METHOD(CreateHistogramAsm)
        {
            PBYTE Buffer;
            ULONG BufferSize = 1 << 16; // 64 KB
            LONG_STRING String;
            BOOLEAN Result;
            CHARACTER_HISTOGRAM HistogramA;
            CHARACTER_HISTOGRAM_V4 HistogramB;
            PCHARACTER_HISTOGRAM Histogram1;
            PCHARACTER_HISTOGRAM Histogram2;
            RTL_GENERIC_COMPARE_RESULTS Comparison;

            ZeroStruct(HistogramA);
            ZeroStruct(HistogramB);

            Histogram1 = &HistogramA;
            Histogram2 = &HistogramB.Histogram1;

            Assert::IsTrue(
                MakeRandomString(Rtl,
                                 Allocator,
                                 BufferSize,
                                 &Buffer)
            );

            String.Length = 64;
            String.Hash = 0;
            String.Buffer = Buffer;

            CopyMemory(String.Buffer, (PVOID)QuickLazyString.Buffer, 64);
            String.Buffer[64] = '\0';

            Assert::IsTrue(Api->CreateHistogram(&String, &HistogramA));
            Assert::IsTrue(
                Api->CreateHistogramAvx2C(&String,
                                          &HistogramB.Histogram1,
                                          &HistogramB.Histogram2)
            );

            Comparison = Api->CompareHistograms(Histogram1,
                                                Histogram2);

            Assert::IsTrue(Comparison == GenericEqual);

            //
            // CreateHistogramAvx2AlignedCV4
            //

            ZeroStruct(HistogramB);

            Assert::IsTrue(
                Api->CreateHistogramAvx2AlignedCV4(&String,
                                                   &HistogramB)
            );

            Comparison = Api->CompareHistograms(Histogram1,
                                                Histogram2);

            Assert::IsTrue(Comparison == GenericEqual);

            //
            // CreateHistogramAvx2AlignedAsm
            //

            ZeroStruct(HistogramB);

            Result = Api->CreateHistogramAvx2AlignedAsm(&String,
                                                        &HistogramB);

            Comparison = Api->CompareHistograms(Histogram1,
                                                Histogram2);

            Assert::IsTrue(Comparison == GenericEqual);


            Allocator->FreePointer(Allocator, (PPVOID)&Buffer);
        }

        TEST_METHOD(CreateHistogram3LongStringAllMethods)
        {
            PBYTE Buffer;
            ULONG BufferSize = 1 << 16; // 64 KB
            LONG_STRING String;
            BOOLEAN Result;
            CHARACTER_HISTOGRAM HistogramA;
            CHARACTER_HISTOGRAM_V4 HistogramB;
            PCHARACTER_HISTOGRAM Histogram1;
            PCHARACTER_HISTOGRAM Histogram2;
            RTL_GENERIC_COMPARE_RESULTS Comparison;

            ZeroStruct(HistogramA);
            ZeroStruct(HistogramB);

            Histogram1 = &HistogramA;
            Histogram2 = &HistogramB.Histogram1;

            Assert::IsTrue(
                MakeRandomString(Rtl,
                                 Allocator,
                                 BufferSize,
                                 &Buffer)
            );

            String.Length = BufferSize;
            String.Hash = 0;
            String.Buffer = Buffer;

            Assert::IsTrue(Api->CreateHistogram(&String, &HistogramA));
            Assert::IsTrue(
                Api->CreateHistogramAvx2C(&String,
                                          &HistogramB.Histogram1,
                                          &HistogramB.Histogram2)
            );

            //
            // CreateHistogramAvx2AlignedCV4
            //

            Comparison = Api->CompareHistograms(Histogram1,
                                                Histogram2);

            ZeroStruct(HistogramB);

            Assert::IsTrue(
                Api->CreateHistogramAvx2AlignedCV4(&String,
                                                   &HistogramB)
            );

            Comparison = Api->CompareHistograms(Histogram1,
                                                Histogram2);

            Assert::IsTrue(Comparison == GenericEqual);

            //
            // CreateHistogramAvx2AlignedAsm
            //

            ZeroStruct(HistogramB);

            Result = Api->CreateHistogramAvx2AlignedAsm(&String,
                                                        &HistogramB);

            Comparison = Api->CompareHistograms(Histogram1,
                                                Histogram2);

            Assert::IsTrue(Comparison == GenericEqual);


            Allocator->FreePointer(Allocator, (PPVOID)&Buffer);
        }

        TEST_METHOD(CreateHistogram16MBString)
        {
            PBYTE Buffer;
            ULONG BufferSize = 1 << 24; // 16 MB
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

            Assert::IsTrue(
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
            Result1 = CreateHistogramInline(&String, &HistogramA);
            QueryPerformanceCounter(&End1);

            Elapsed1.QuadPart = End1.QuadPart - Start1.QuadPart;

            Assert::IsTrue(Result1);

            QueryPerformanceCounter(&Start2);
            Result2 = CreateHistogramAvx2Inline(&String,
                                                &HistogramB.Histogram1,
                                                &HistogramB.Histogram2);
            QueryPerformanceCounter(&End2);

            Elapsed2.QuadPart = End2.QuadPart - Start2.QuadPart;

            Assert::IsTrue(Result2);

            Histogram1 = &HistogramA;
            Histogram2 = &HistogramB.Histogram1;

            Comparison = CompareHistogramsAlignedAvx2Inline(Histogram1,
                                                            Histogram2);


            Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

            Assert::IsTrue(Comparison == GenericEqual);
        }

        TEST_METHOD(AddWordNullStringRejected)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            Assert::IsFalse(
                Api->AddWord(Dictionary,
                             (PCBYTE)"",
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 0);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(AddWordRejectsShortWord)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            Assert::IsTrue(Api->SetMinimumWordLength(Dictionary, 2));

            Assert::IsFalse(
                Api->AddWord(Dictionary,
                             (PCBYTE)"a",
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 0);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(AddWordRejectsLongWord)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            Assert::IsTrue(Api->SetMaximumWordLength(Dictionary, 2));

            Assert::IsFalse(
                Api->AddWord(Dictionary,
                             (PCBYTE)"abc",
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 0);

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
                             Elbow,
                             &EntryCount)
            );

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             Elbow,
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

        TEST_METHOD(GetDictionaryStats1)
        {
            LONGLONG EntryCount;
            PDICTIONARY Dictionary;
            PDICTIONARY_STATS Stats;
            DICTIONARY_CREATE_FLAGS CreateFlags;
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
                Api->AddWord(Dictionary,
                             Elbow,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->GetDictionaryStats(Dictionary,
                                        Allocator,
                                        &Stats)
            );

            Assert::IsFalse(
                strncmp((PCSZ)Elbow,
                        (PCSZ)Stats->CurrentLongestWord->Buffer,
                        ElbowLength)
            );

            Assert::AreEqual(
                (PCSZ)Elbow,
                (PCSZ)Stats->CurrentLongestWord->Buffer
            );

            Assert::AreEqual(
                (PCSZ)Elbow,
                (PCSZ)Stats->LongestWordAllTime->Buffer
            );

            Allocator->FreePointer(Allocator, (PPVOID)&Stats);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(GetWordAnagrams1)
        {
            LONGLONG EntryCount;
            PLIST_ENTRY ListEntry;
            PDICTIONARY Dictionary;
            PCWORD_ENTRY WordEntry;
            PLINKED_WORD_LIST LinkedWordList;
            PLINKED_WORD_ENTRY LinkedWordEntry;

            BOOLEAN IsProcessTerminating;
            DICTIONARY_CREATE_FLAGS CreateFlags;


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
                             Elbow,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->GetWordAnagrams(Dictionary,
                                     Allocator,
                                     Elbow,
                                     &LinkedWordList)
            );

            Assert::IsTrue(LinkedWordList == NULL);

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             Below,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->GetWordAnagrams(Dictionary,
                                     Allocator,
                                     Elbow,
                                     &LinkedWordList)
            );

            Assert::IsTrue(LinkedWordList != NULL);
            Assert::IsTrue(LinkedWordList->NumberOfEntries == 1);
            Assert::IsFalse(IsListEmpty(&LinkedWordList->ListHead));

            ListEntry = RemoveHeadList(&LinkedWordList->ListHead);

            LinkedWordEntry = CONTAINING_RECORD(ListEntry,
                                                LINKED_WORD_ENTRY,
                                                ListEntry);

            WordEntry = &LinkedWordEntry->WordEntry;

            Assert::AreEqual(
                (PCSZ)Below,
                (PCSZ)WordEntry->String.Buffer
            );

            Assert::IsTrue(IsListEmpty(&LinkedWordList->ListHead));

            Allocator->FreePointer(Allocator, (PPVOID)&LinkedWordList);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(FindWord1)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;
            BOOLEAN Exists;
            WORD_STATS Stats;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            //
            // Verify parameter validation.
            //

            Assert::IsFalse(Api->FindWord(NULL, NULL, NULL));
            Assert::IsFalse(Api->FindWord(Dictionary, NULL, NULL));
            Assert::IsFalse(Api->FindWord(Dictionary, Elbow, NULL));

            //
            // Test "elbow".
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->GetWordStats(Dictionary, Elbow, &Stats));
            Assert::IsTrue(Stats.EntryCount == 2);
            Assert::IsTrue(Stats.MaximumEntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsTrue(Exists);

            //
            // Test "below".
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Below, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Below, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, Below, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Below, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, Below, &Exists));
            Assert::IsTrue(Exists);

            //
            // Test quick fox...
            //

            Assert::IsTrue(Api->FindWord(Dictionary, QuickFox, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, QuickFox, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, QuickFox, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, QuickFox, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, QuickFox, &Exists));
            Assert::IsTrue(Exists);

            //
            // Test lazy dog...
            //

            Assert::IsTrue(Api->FindWord(Dictionary, LazyDog, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, LazyDog, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, LazyDog, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, LazyDog, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, LazyDog, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(RemoveWord1)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN Exists;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            //
            // Verify parameter validation.
            //

            Assert::IsFalse(Api->RemoveWord(NULL, NULL, NULL));
            Assert::IsFalse(Api->RemoveWord(Dictionary, NULL, NULL));
            Assert::IsFalse(Api->RemoveWord(Dictionary, Elbow, NULL));

            //
            // Verify removing a word that doesn't exist.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Add elbow twice.
            //

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            //
            // Verify removing below results in an error.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Below, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Remove elbow and verify one still exists.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            //
            // Verify we can still find it.
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsTrue(Exists);

            //
            // Add it again and verify the entry count is 2.
            //

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(RemoveWord2)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN Exists;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;
            WORD_STATS Stats;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            //
            // Verify parameter validation.
            //

            Assert::IsFalse(Api->RemoveWord(NULL, NULL, NULL));
            Assert::IsFalse(Api->RemoveWord(Dictionary, NULL, NULL));
            Assert::IsFalse(Api->RemoveWord(Dictionary, Elbow, NULL));

            //
            // Verify removing a word that doesn't exist.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Add elbow twice.
            //

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->GetWordStats(Dictionary, Elbow, &Stats));
            Assert::IsTrue(Stats.EntryCount == 2);
            Assert::IsTrue(Stats.MaximumEntryCount == 2);

            //
            // Verify removing below results in an error.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Below, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Remove elbow and verify one still exists.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->GetWordStats(Dictionary, Elbow, &Stats));
            Assert::IsTrue(Stats.EntryCount == 1);
            Assert::IsTrue(Stats.MaximumEntryCount == 2);

            //
            // Verify we can still find it.
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsTrue(Exists);

            //
            // Remove elbow.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 0);

            //
            // Try remove it again and verify we get -1.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Try find it again and verify it's not there.
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsFalse(Exists);


            Assert::IsTrue(
                Api->DestroyDictionary(
                    &Dictionary,
                    &IsProcessTerminating
                )
            );
        }

        TEST_METHOD(RemoveWord3)
        {
            DICTIONARY_CREATE_FLAGS CreateFlags;
            PDICTIONARY Dictionary;
            BOOLEAN Exists;
            BOOLEAN IsProcessTerminating;
            LONGLONG EntryCount;

            CreateFlags.AsULong = 0;
            IsProcessTerminating = TRUE;

            Assert::IsTrue(
                Api->CreateDictionary(Rtl,
                                      Allocator,
                                      CreateFlags,
                                      &Dictionary)
            );

            //
            // Verify parameter validation.
            //

            Assert::IsFalse(Api->RemoveWord(NULL, NULL, NULL));
            Assert::IsFalse(Api->RemoveWord(Dictionary, NULL, NULL));
            Assert::IsFalse(Api->RemoveWord(Dictionary, Elbow, NULL));

            //
            // Verify removing a word that doesn't exist.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Add elbow twice.
            //

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(Api->AddWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            //
            // Verify removing below results in an error.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Below, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Remove elbow and verify one still exists.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            //
            // Verify we can still find it.
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsTrue(Exists);

            //
            // Add a new longest word.
            //

            Assert::IsTrue(Api->AddWord(Dictionary, QuickFox, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            //
            // Remove elbow.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == 0);

            //
            // Try remove it again and verify we get -1.
            //

            Assert::IsTrue(Api->RemoveWord(Dictionary, Elbow, &EntryCount));
            Assert::IsTrue(EntryCount == -1);

            //
            // Try find it again and verify it's not there.
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow, &Exists));
            Assert::IsFalse(Exists);


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
