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
                             Elbow.Buffer,
                             &EntryCount)
            );

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             Elbow.Buffer,
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
                             Elbow.Buffer,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->GetDictionaryStats(Dictionary,
                                        Allocator,
                                        &Stats)
            );

            Assert::IsFalse(
                strncmp((PCSZ)Elbow.Buffer,
                        (PCSZ)Stats->CurrentLongestWord->Buffer,
                        Elbow.Length)
            );

            Assert::AreEqual(
                (PCSZ)Elbow.Buffer,
                (PCSZ)Stats->CurrentLongestWord->Buffer
            );

            Assert::AreEqual(
                (PCSZ)Elbow.Buffer,
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
                             Elbow.Buffer,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->GetWordAnagrams(Dictionary,
                                     Allocator,
                                     Elbow.Buffer,
                                     &LinkedWordList)
            );

            Assert::IsTrue(LinkedWordList == NULL);

            Assert::IsTrue(
                Api->AddWord(Dictionary,
                             Below.Buffer,
                             &EntryCount)
            );

            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(
                Api->GetWordAnagrams(Dictionary,
                                     Allocator,
                                     Elbow.Buffer,
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
                (PCSZ)Below.Buffer,
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
            PCBYTE Fox = QuickFox.Buffer;
            PCBYTE Dog = LazyDog.Buffer;

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
            Assert::IsFalse(Api->FindWord(Dictionary, Elbow.Buffer, NULL));

            //
            // Test "elbow".
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow.Buffer, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow.Buffer, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow.Buffer, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Elbow.Buffer, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, Elbow.Buffer, &Exists));
            Assert::IsTrue(Exists);

            //
            // Test "below".
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Below.Buffer, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Below.Buffer, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, Below.Buffer, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Below.Buffer, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, Below.Buffer, &Exists));
            Assert::IsTrue(Exists);

            //
            // Test quick fox...
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Fox, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Fox, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, Fox, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Fox, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, Fox, &Exists));
            Assert::IsTrue(Exists);

            //
            // Test lazy dog...
            //

            Assert::IsTrue(Api->FindWord(Dictionary, Dog, &Exists));
            Assert::IsFalse(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Dog, &EntryCount));
            Assert::IsTrue(EntryCount == 1);

            Assert::IsTrue(Api->FindWord(Dictionary, Dog, &Exists));
            Assert::IsTrue(Exists);

            Assert::IsTrue(Api->AddWord(Dictionary, Dog, &EntryCount));
            Assert::IsTrue(EntryCount == 2);

            Assert::IsTrue(Api->FindWord(Dictionary, Dog, &Exists));
            Assert::IsTrue(Exists);

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
