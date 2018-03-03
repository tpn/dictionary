/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Dictionary.h

Abstract:

    This is the main header file for the Dictionary component.

--*/

#ifdef _DICTIONARY_INTERNAL_BUILD

//
// This is an internal build of the TracerHeap component.
//

#ifdef _DICTIONARY_DLL_BUILD

//
// This is the DLL build.
//

#define DICTIONARY_API __declspec(dllexport)
#define DICTIONARY_DATA extern __declspec(dllexport)

#else

//
// This is the static library build.
//

#define DICTIONARY_API
#define DICTIONARY_DATA

#endif

#include "stdafx.h"

#else

#ifdef _DICTIONARY_STATIC_LIB

//
// We're being included by another project as a static library.
//

#define DICTIONARY_API
#define DICTIONARY_DATA extern

#else

//
// We're being included by an external component that wants to use us as a DLL.
//

#define DICTIONARY_API __declspec(dllimport)
#define DICTIONARY_DATA extern __declspec(dllimport)

#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "../Rtl/Rtl.h"

#endif

//
// Define an opaque DICTIONARY structure.
//

typedef struct _DICTIONARY DICTIONARY;
typedef DICTIONARY *PDICTIONARY;
typedef const DICTIONARY *PCDICTIONARY;

//
// We can't use STRING structures here as the words might be up to 1MB and we
// can't represent that size via the USHORT Length parameters.  So, use a new
// structure named LONG_STRING with appropriate ULONG sizes.
//

typedef struct _LONG_STRING {

    //
    // Length of the string, in bytes.
    //

    ULONG Length;

    //
    // Hash of the string.
    //

    ULONG Hash;

    //
    // Pointer to the string.
    //

    union {
        PCHAR AsCharBuffer;
        PBYTE Buffer;
    };

} LONG_STRING;
typedef LONG_STRING *PLONG_STRING;
typedef const LONG_STRING *PCLONG_STRING;

//
// Define a macro to help initialize static C variables to long strings.
// (This is equivalent to RTL_CONSTANT_STRING.)
//

#define CONSTANT_LONG_STRING(s) { \
    sizeof(s) - sizeof((s)[0]),   \
    0,                            \
    s                             \
}

//
// Define the WORD_STATS structure to capture entry count and maximum entry
// count.
//

typedef struct _WORD_STATS {

    //
    // Number of duplicate instances of the word in the dictionary.
    //

    LONGLONG EntryCount;

    //
    // Highest number of duplicate instances ever.
    //

    LONGLONG MaximumEntryCount;

} WORD_STATS;
typedef WORD_STATS *PWORD_STATS;

//
// Define the WORD_ENTRY structure used to capture words.
//

typedef struct _WORD_ENTRY {

    //
    // Statistics about the underlying word with relation to the dictionary.
    //

    WORD_STATS Stats;

    //
    // The string representation of this word entry.
    //

    LONG_STRING String;

} WORD_ENTRY;
typedef WORD_ENTRY *PWORD_ENTRY;
typedef const WORD_ENTRY *PCWORD_ENTRY;


//
// Define the LINKED_WORD_LIST structure used to link anagrams together.
//

typedef struct _LINKED_WORD_LIST {

    //
    // Number of entries in the list.
    //

    LONGLONG NumberOfEntries;

    //
    // List head used to chain the underlying LINKED_WORD_ENTRY structures
    // together via their ListEntry field.
    //

    LIST_ENTRY ListHead;

} LINKED_WORD_LIST;
typedef LINKED_WORD_LIST *PLINKED_WORD_LIST;

//
// Define the LINKED_WORD_ENTRY structure, which is used to chain anagrams
// together via the LINKED_WORD_LIST structure.
//

typedef struct _LINKED_WORD_ENTRY {

    //
    // List entry used to append the word entry to the LINKED_WORD_LIST's
    // ListHead field.
    //

    LIST_ENTRY ListEntry;

    //
    // Pointer to the word entry;
    //

    WORD_ENTRY WordEntry;

} LINKED_WORD_ENTRY;
typedef LINKED_WORD_ENTRY *PLINKED_WORD_ENTRY;
typedef const LINKED_WORD_ENTRY *PCLINKED_WORD_ENTRY;

//
// Define the DICTIONARY_STATS interface.
//

typedef struct _DICTIONARY_STATS {

    //
    // Current longest word in the dictionary.
    //

    PCLONG_STRING CurrentLongestWord;

    //
    // Longest word of all time ever entered into the dictionary.
    //

    PCLONG_STRING LongestWordAllTime;

} DICTIONARY_STATS;
typedef DICTIONARY_STATS *PDICTIONARY_STATS;

//
// Define the DICTIONARY interface function pointers.
//

typedef union _DICTIONARY_CREATE_FLAGS {
    struct {
        ULONG Unused:32;
    };
    LONG AsLong;
    ULONG AsULong;
} DICTIONARY_CREATE_FLAGS;
typedef DICTIONARY_CREATE_FLAGS *PDICTIONARY_CREATE_FLAGS;
C_ASSERT(sizeof(DICTIONARY_CREATE_FLAGS) == sizeof(ULONG));

typedef
_Check_return_
_Success_(return != 0)
BOOLEAN
(NTAPI CREATE_DICTIONARY)(
    _In_ PRTL Rtl,
    _In_ PALLOCATOR Allocator,
    _In_opt_ DICTIONARY_CREATE_FLAGS CreateFlags,
    _Outptr_result_nullonfailure_ PDICTIONARY *Dictionary
    );
typedef CREATE_DICTIONARY *PCREATE_DICTIONARY;

typedef
BOOLEAN
(NTAPI DESTROY_DICTIONARY)(
    _Pre_notnull_ _Post_satisfies_(*DictionaryPointer == 0)
        PDICTIONARY *DictionaryPointer,
    _In_opt_ PBOOLEAN IsProcessTerminating
    );
typedef DESTROY_DICTIONARY *PDESTROY_DICTIONARY;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI ADD_WORD)(
    _Inout_ PDICTIONARY Dictionary,
    _In_z_ PCBYTE Word,
    _Out_ LONGLONG *EntryCountPointer
    );
typedef ADD_WORD *PADD_WORD;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI FIND_WORD)(
    _In_ PDICTIONARY Dictionary,
    _In_z_ PCBYTE Word,
    _Out_ PBOOLEAN Exists
    );
typedef FIND_WORD *PFIND_WORD;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI REMOVE_WORD)(
    _Inout_ PDICTIONARY Dictionary,
    _In_z_ PCBYTE Word,
    _Out_ LONGLONG *EntryCountPointer
    );
typedef REMOVE_WORD *PREMOVE_WORD;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI GET_WORD_ANAGRAMS)(
    _In_ PDICTIONARY Dictionary,
    _In_ PALLOCATOR Allocator,
    _In_z_ PCBYTE Word,
    _Out_ PLINKED_WORD_LIST *LinkedWordListPointer
    );
typedef GET_WORD_ANAGRAMS *PGET_WORD_ANAGRAMS;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI GET_WORD_STATS)(
    _In_ PDICTIONARY Dictionary,
    _In_z_ PCBYTE Word,
    _Out_ PWORD_STATS Stats
    );
typedef GET_WORD_STATS *PGET_WORD_STATS;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI GET_DICTIONARY_STATS)(
    _In_ PDICTIONARY Dictionary,
    _In_ PALLOCATOR Allocator,
    _Out_ PDICTIONARY_STATS *DictionaryStatsPointer
    );
typedef GET_DICTIONARY_STATS *PGET_DICTIONARY_STATS;

//
// Helper functions (useful for unit tests).
//

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI COMPARE_WORDS)(
    _In_ _Const_ PCLONG_STRING LeftString,
    _In_ _Const_ PCLONG_STRING RightString
    );
typedef COMPARE_WORDS *PCOMPARE_WORDS;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI SET_MINIMUM_WORD_LENGTH)(
    _Inout_ PDICTIONARY Dictionary,
    _In_ ULONG MinimumWordLength
    );
typedef SET_MINIMUM_WORD_LENGTH *PSET_MINIMUM_WORD_LENGTH;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI SET_MAXIMUM_WORD_LENGTH)(
    _Inout_ PDICTIONARY Dictionary,
    _In_ ULONG MinimumWordLength
    );
typedef SET_MAXIMUM_WORD_LENGTH *PSET_MAXIMUM_WORD_LENGTH;

//
// Internal functionality exposed to facilitate benchmarking.
//

#define NUMBER_OF_CHARACTER_BITS 256

typedef union DECLSPEC_ALIGN(64) _CHARACTER_HISTOGRAM {
    YMMWORD Ymm[32];
    XMMWORD Xmm[64];
    ULONG Counts[NUMBER_OF_CHARACTER_BITS];
} CHARACTER_HISTOGRAM;
C_ASSERT(sizeof(CHARACTER_HISTOGRAM) == 1024);
typedef CHARACTER_HISTOGRAM *PCHARACTER_HISTOGRAM;
typedef const CHARACTER_HISTOGRAM *PCCHARACTER_HISTOGRAM;

typedef struct DECLSPEC_ALIGN(64) _CHARACTER_HISTOGRAM_V4 {
    CHARACTER_HISTOGRAM Histogram1;
    CHARACTER_HISTOGRAM Histogram2;
    CHARACTER_HISTOGRAM Histogram3;
    CHARACTER_HISTOGRAM Histogram4;
} CHARACTER_HISTOGRAM_V4;
C_ASSERT(sizeof(CHARACTER_HISTOGRAM_V4) == 4096);
typedef CHARACTER_HISTOGRAM_V4 *PCHARACTER_HISTOGRAM_V4;

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI COMPARE_HISTOGRAMS)(
    _In_ PCCHARACTER_HISTOGRAM Left,
    _In_ PCCHARACTER_HISTOGRAM Right
    );
typedef COMPARE_HISTOGRAMS *PCOMPARE_HISTOGRAMS;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI CREATE_HISTOGRAM)(
    _In_ PCLONG_STRING String,
    _Inout_updates_bytes_(sizeof(*Histogram)) PCHARACTER_HISTOGRAM Histogram
    );
typedef CREATE_HISTOGRAM *PCREATE_HISTOGRAM;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI CREATE_HISTOGRAM2)(
    _In_ PCLONG_STRING String,
    _Inout_updates_bytes_(sizeof(*Histogram))
        PCHARACTER_HISTOGRAM Histogram,
    _Inout_updates_bytes_(sizeof(*TempHistogram))
        PCHARACTER_HISTOGRAM TempHistogram
    );
typedef CREATE_HISTOGRAM2 *PCREATE_HISTOGRAM2;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI CREATE_HISTOGRAM_V4)(
    _In_ PCLONG_STRING String,
    _Inout_updates_bytes_(sizeof(*Histogram))
        PCHARACTER_HISTOGRAM_V4 Histogram
    );
typedef CREATE_HISTOGRAM_V4 *PCREATE_HISTOGRAM_V4;

//
// Define the main dictionary API structure.
//

typedef struct _DICTIONARY_FUNCTIONS {

    //
    // Dictionary creation and destruction methods.
    //

    PCREATE_DICTIONARY CreateDictionary;
    PDESTROY_DICTIONARY DestroyDictionary;

    //
    // Main API methods.
    //

    PADD_WORD AddWord;
    PFIND_WORD FindWord;
    PREMOVE_WORD RemoveWord;
    PGET_WORD_STATS GetWordStats;
    PGET_WORD_ANAGRAMS GetWordAnagrams;
    PGET_DICTIONARY_STATS GetDictionaryStats;

    //
    // Helpers.
    //

    PCOMPARE_WORDS CompareWords;

    PSET_MINIMUM_WORD_LENGTH SetMinimumWordLength;
    PSET_MAXIMUM_WORD_LENGTH SetMaximumWordLength;

    //
    // Internal functionality exposed to facilitate benchmarking.
    //

    PCOMPARE_HISTOGRAMS CompareHistograms;
    PCREATE_HISTOGRAM CreateHistogram;
    PCREATE_HISTOGRAM2 CreateHistogramAvx2C;
    PCREATE_HISTOGRAM2 CreateHistogramAvx2AlignedC;
    PCREATE_HISTOGRAM2 CreateHistogramAvx2AlignedC32;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v2;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v3;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v4;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v5;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v5_2;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v5_3;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v5_3_2;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedAsm_v5_3_3;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx2AlignedCV4;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx512AlignedAsm;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx512AlignedAsm_v2;
    PCREATE_HISTOGRAM_V4 CreateHistogramAvx512AlignedAsm_v3;

} DICTIONARY_FUNCTIONS;
typedef DICTIONARY_FUNCTIONS *PDICTIONARY_FUNCTIONS;

//
// Inline function for helping load the dictionary in a dynamic module context.
// See ScratchExe/main.c or TestDictionary/unittest1.cpp for examples.
//

FORCEINLINE
BOOLEAN
LoadDictionaryModule(
    PRTL Rtl,
    HMODULE *ModulePointer,
    PDICTIONARY_FUNCTIONS Functions
    )
{
    BOOL Success;
    HMODULE Module;
    ULONG NumberOfResolvedSymbols;
    ULONG ExpectedNumberOfResolvedSymbols;

    CONST PCSTR Names[] = {
        "CreateDictionary",
        "DestroyDictionary",

        "AddWord",
        "FindWord",
        "RemoveWord",
        "GetWordStats",
        "GetWordAnagrams",
        "GetDictionaryStats",

        "CompareWords",
        "SetMinimumWordLength",
        "SetMaximumWordLength",

        "CompareHistograms",
        "CreateHistogram",
        "CreateHistogramAvx2C",
        "CreateHistogramAvx2AlignedC",
        "CreateHistogramAvx2AlignedC32",
        "CreateHistogramAvx2AlignedAsm",
        "CreateHistogramAvx2AlignedAsm_v2",
        "CreateHistogramAvx2AlignedAsm_v3",
        "CreateHistogramAvx2AlignedAsm_v4",
        "CreateHistogramAvx2AlignedAsm_v5",
        "CreateHistogramAvx2AlignedAsm_v5_2",
        "CreateHistogramAvx2AlignedAsm_v5_3",
        "CreateHistogramAvx2AlignedAsm_v5_3_2",
        "CreateHistogramAvx2AlignedAsm_v5_3_3",
        "CreateHistogramAvx2AlignedCV4",
        "CreateHistogramAvx512AlignedAsm",
        "CreateHistogramAvx512AlignedAsm_v2",
        "CreateHistogramAvx512AlignedAsm_v3",

    };

    ULONG BitmapBuffer[(ALIGN_UP(ARRAYSIZE(Names), sizeof(ULONG) << 3) >> 5)+1];
    RTL_BITMAP FailedBitmap = { ARRAYSIZE(Names)+1, (PULONG)&BitmapBuffer };

    ExpectedNumberOfResolvedSymbols = ARRAYSIZE(Names);

    Module = LoadLibraryA("Dictionary.dll");
    if (!Module) {
        return FALSE;
    }

    Success = Rtl->LoadSymbols(
        Names,
        ARRAYSIZE(Names),
        (PULONG_PTR)Functions,
        sizeof(*Functions) / sizeof(ULONG_PTR),
        Module,
        &FailedBitmap,
        TRUE,
        &NumberOfResolvedSymbols
    );

    if (!Success) {
        __debugbreak();
    }

    if (ExpectedNumberOfResolvedSymbols != NumberOfResolvedSymbols) {
        PCSTR FirstFailedSymbolName;
        ULONG FirstFailedSymbol;
        ULONG NumberOfFailedSymbols;

        NumberOfFailedSymbols = Rtl->RtlNumberOfSetBits(&FailedBitmap);
        FirstFailedSymbol = Rtl->RtlFindSetBits(&FailedBitmap, 1, 0);
        FirstFailedSymbolName = Names[FirstFailedSymbol-1];
        __debugbreak();
    }

    *ModulePointer = Module;

    return TRUE;
}

#ifdef __cplusplus
} // extern "C"
#endif

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
