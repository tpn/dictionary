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

#ifndef ASSERT
#define ASSERT(Condition)   \
    if (!(Condition)) {     \
        __debugbreak();     \
    }
#endif

//
// Use the slim R/W locks for the dictionary structure, as they pair nicely
// with AVL tables, which don't splay during read-only operations (as opposed
// to the RTL_GENERIC_TABLE counterpart).  This means we can provide safe
// multithreaded access for multiple readers to a) query the tree for the
// presence of a word, and b) enumerate all anagrams for a given word.  Any
// operations that mutate the underlying trees (removing a word, destroying
// the dictionary) require the exclusive lock to be held.
//
// N.B. SAL concurrency annotations are used where applicable to ensure correct
//      lock usage.
//

typedef SRWLOCK DICTIONARY_LOCK;
#define InitializeDictionaryLock InitializeSRWLock
#define AcquireDictionaryLockShared AcquireSRWLockShared
#define AcquireDictionaryLockExclusive AcquireSRWLockExclusive
#define ReleaseDictionaryLockShared ReleaseSRWLockShared
#define ReleaseDictionaryLockExclusive ReleaseSRWLockExclusive

//
// Define character bitmap and histogram structures and supporting function
// typedefs.
//

#define NUMBER_OF_CHARACTER_BITS 256
#define NUMBER_OF_CHARACTER_BITS_IN_BYTES (256 / 8)
#define NUMBER_OF_CHARACTER_BITS_IN_DOUBLEWORDS (256 / (4 << 3))
#define NUMBER_OF_CHARACTER_BITS_IN_QUADWORDS (256 / (8 << 3))

typedef union DECLSPEC_ALIGN(32) _CHARACTER_BITMAP {
     YMMWORD Ymm;
     LONG Bits[NUMBER_OF_CHARACTER_BITS_IN_DOUBLEWORDS];
} CHARACTER_BITMAP;
typedef CHARACTER_BITMAP *PCHARACTER_BITMAP;
typedef const CHARACTER_BITMAP *PCCHARACTER_BITMAP;

typedef struct DECLSPEC_ALIGN(256) _CHARACTER_HISTOGRAM {
    YMMWORD Ymm[32];
    ULONG Counts[NUMBER_OF_CHARACTER_BITS];
} CHARACTER_HISTOGRAM;
typedef CHARACTER_HISTOGRAM *PCHARACTER_HISTOGRAM;
typedef const CHARACTER_HISTOGRAM *PCCHARACTER_HISTOGRAM;

typedef
_Success_(return != 0)
BOOLEAN
(NTAPI INITIALIZE_WORD)(
    _In_z_ PCBYTE Word,
    _In_ ULONG MinimumLength,
    _In_ ULONG MaximumLength,
    _Inout_ PLONG_STRING String,
    _Inout_ PCHARACTER_BITMAP Bitmap,
    _Inout_ PCHARACTER_HISTOGRAM Histogram,
    _Out_ PULONG BitmapHashPointer,
    _Out_ PULONG HistogramHashPointer
    );
typedef INITIALIZE_WORD *PINITIALIZE_WORD;
extern INITIALIZE_WORD InitializeWord;

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI COMPARE_CHARACTER_HISTOGRAMS)(
    _In_ _Const_ PCCHARACTER_HISTOGRAM Left,
    _In_ _Const_ PCCHARACTER_HISTOGRAM Right
    );
typedef COMPARE_CHARACTER_HISTOGRAMS *PCOMPARE_CHARACTER_HISTOGRAMS;

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI CONFIRM_GENERIC_EQUAL)(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    );


//
// Define the word table.  This is the third and final tier of the dictionary's
// data structure hierarchy.  Each entry is keyed by the underlying LONG_STRING
// representing the given word.
//

typedef struct _WORD_TABLE {
    RTL_AVL_TABLE Avl;
    LIST_ENTRY AnagramListHead;
} WORD_TABLE;
typedef WORD_TABLE *PWORD_TABLE;

typedef struct _WORD_TABLE_ENTRY {
    struct _LENGTH_TABLE_ENTRY *LengthEntry;
    WORD_ENTRY WordEntry;
} WORD_TABLE_ENTRY;
typedef WORD_TABLE_ENTRY *PWORD_TABLE_ENTRY;

typedef struct _WORD_TABLE_ENTRY_FULL {

    union {

        //
        // Inline RTL_BALANCED_LINKS structure and abuse the ULONG at the end
        // of the structure normally used for padding for our hash.
        //

        struct {

            struct _RTL_BALANCED_LINKS *Parent;
            struct _RTL_BALANCED_LINKS *LeftChild;
            struct _RTL_BALANCED_LINKS *RightChild;
            CHAR Balance;
            UCHAR Reserved[3];

            //
            // Stash the hash of the word here. for the bitmap here.
            //

            ULONG Hash;
        };

        RTL_BALANCED_LINKS BalancedLinks;
    };

    WORD_TABLE_ENTRY Entry;
    struct _LENGTH_TABLE_ENTRY *LengthEntry;

} WORD_TABLE_ENTRY_FULL;
typedef WORD_TABLE_ENTRY_FULL *PWORD_TABLE_ENTRY_FULL;

//
// Define the length table.
//

typedef struct _LENGTH_TABLE {
    RTL_AVL_TABLE Avl;
} LENGTH_TABLE;
typedef LENGTH_TABLE *PLENGTH_TABLE;

typedef struct _LENGTH_TABLE_ENTRY {
    LIST_ENTRY LengthListEntry;
    PWORD_ENTRY WordEntry;
} LENGTH_TABLE_ENTRY;
typedef LENGTH_TABLE_ENTRY *PLENGTH_TABLE_ENTRY;

typedef struct _LENGTH_TABLE_ENTRY_FULL {

    union {

        //
        // Inline RTL_BALANCED_LINKS structure and abuse the ULONG at the end
        // of the structure normally used for padding for our hash.
        //

        struct {

            struct _RTL_BALANCED_LINKS *Parent;
            struct _RTL_BALANCED_LINKS *LeftChild;
            struct _RTL_BALANCED_LINKS *RightChild;
            CHAR Balance;
            UCHAR Reserved[3];

            //
            // Stash the word length here.
            //

            ULONG Length;
        };

        RTL_BALANCED_LINKS BalancedLinks;
    };

    LENGTH_TABLE_ENTRY Entry;

} LENGTH_TABLE_ENTRY_FULL;
typedef LENGTH_TABLE_ENTRY_FULL *PLENGTH_TABLE_ENTRY_FULL;


//
// Define the histogram table.  This is the second tier of the dictionary's
// data structure hierarchy.  Each entry is uniquely-keyed by a character
// histogram for a given set of word entries.  (The histogram is essentially
// an array of 256 ULONGs; each element in the array reflects the character
// value at that offset, e.g. the letter 'f' would be at offset 102, and the
// underlying ULONG would represent a count of how many times 'f' appears in
// the given word.)
//
// This allows us to link anagrams together relatively efficiently as new words
// are added to the table.
//


typedef struct _HISTOGRAM_TABLE {
    RTL_AVL_TABLE Avl;
} HISTOGRAM_TABLE;
typedef HISTOGRAM_TABLE *PHISTOGRAM_TABLE;

typedef struct _HISTOGRAM_TABLE_ENTRY {
    CHARACTER_HISTOGRAM Histogram;
    WORD_TABLE WordTable;
    LIST_ENTRY AnagramListHead;
} HISTOGRAM_TABLE_ENTRY;
typedef HISTOGRAM_TABLE_ENTRY *PHISTOGRAM_TABLE_ENTRY;

typedef struct _HISTOGRAM_TABLE_ENTRY_FULL {

    union {

        //
        // Inline RTL_BALANCED_LINKS structure and abuse the ULONG at the end
        // of the structure normally used for padding for our hash.
        //

        struct {

            struct _RTL_BALANCED_LINKS *Parent;
            struct _RTL_BALANCED_LINKS *LeftChild;
            struct _RTL_BALANCED_LINKS *RightChild;
            CHAR Balance;
            UCHAR Reserved[3];

            //
            // Stash the hash for the histogram here.
            //

            ULONG Hash;
        };

        RTL_BALANCED_LINKS BalancedLinks;
    };

    //
    // Overlap the RTL table entry header 'UserData' field with our table
    // entry field.
    //

    union {
        ULONGLONG UserData;
        HISTOGRAM_TABLE_ENTRY Entry;
    };

} HISTOGRAM_TABLE_ENTRY_FULL;
typedef HISTOGRAM_TABLE_ENTRY_FULL *PHISTOGRAM_TABLE_ENTRY_FULL;

//
// Define the bitmap table.
//

typedef struct _BITMAP_TABLE {
    RTL_AVL_TABLE Avl;
} BITMAP_TABLE;
typedef BITMAP_TABLE *PBITMAP_TABLE;

typedef struct _BITMAP_TABLE_ENTRY {
    CHARACTER_BITMAP Bitmap;
    HISTOGRAM_TABLE HistogramTable;
} BITMAP_TABLE_ENTRY;
typedef BITMAP_TABLE_ENTRY *PBITMAP_TABLE_ENTRY;

typedef struct _BITMAP_TABLE_ENTRY_FULL {

    union {

        //
        // Inline RTL_BALANCED_LINKS structure and abuse the ULONG at the end
        // of the structure normally used for padding for our hash.
        //

        struct {

            struct _RTL_BALANCED_LINKS *Parent;
            struct _RTL_BALANCED_LINKS *LeftChild;
            struct _RTL_BALANCED_LINKS *RightChild;
            CHAR Balance;
            UCHAR Reserved[3];

            //
            // Stash the hash for the bitmap here.
            //

            ULONG Hash;
        };

        RTL_BALANCED_LINKS BalancedLinks;
    };

    BITMAP_TABLE_ENTRY Entry;

} BITMAP_TABLE_ENTRY_FULL;
typedef BITMAP_TABLE_ENTRY_FULL *PBITMAP_TABLE_ENTRY_FULL;

//
// Define a generic table entry structure.
//

typedef struct _TABLE_ENTRY_FULL {

    union {

        //
        // Inline RTL_BALANCED_LINKS structure and abuse the ULONG at the end
        // of the structure normally used for padding for our hash.
        //

        struct {

            struct _RTL_BALANCED_LINKS *Parent;
            struct _RTL_BALANCED_LINKS *LeftChild;
            struct _RTL_BALANCED_LINKS *RightChild;
            CHAR Balance;
            UCHAR Reserved[3];

            //
            // Stash the hash of the word here. for the bitmap here.
            //

            union {
                ULONG Hash;
                ULONG Value;
                ULONG Length;
            };
        };

        RTL_BALANCED_LINKS BalancedLinks;
    };

    union {
        WORD_TABLE_ENTRY WordTableEntry;
        LENGTH_TABLE_ENTRY LengthTableEntry;
        BITMAP_TABLE_ENTRY BitmapTableEntry;
        HISTOGRAM_TABLE_ENTRY HistogramTableEntry;
    };

} TABLE_ENTRY_FULL;
typedef TABLE_ENTRY_FULL *PTABLE_ENTRY_FULL;


//
// Define the main DICTIONARY structure and supporting flags.
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
typedef DICTIONARY_FLAGS *PDICTIONARY_FLAGS;

#define MINIMUM_WORD_LENGTH  1          // 1 byte
#define MAXIMUM_WORD_LENGTH (1 << 20)   // 1 MB (1048576 bytes)

typedef struct _Struct_size_bytes_(SizeOfStruct) _DICTIONARY {

    //
    // Reserve a slot for a vtable.  Currently unused.
    //

    PPVOID Vtbl;

    //
    // Size of the structure, in bytes.
    //

    _Field_range_(==, sizeof(struct _DICTIONARY)) ULONG SizeOfStruct;

    //
    // Dictionary flags.
    //

    DICTIONARY_FLAGS Flags;

    //
    // Minimum and maximum word lengths, in bytes.
    //

    ULONG MinimumWordLength;
    ULONG MaximumWordLength;

    //
    // Pointer to an initialized RTL structure.
    //

    PRTL Rtl;

    //
    // Pointer to an initialized ALLOCATOR structure.
    //

    PALLOCATOR Allocator;

    //
    // Pointers to individual table allocators.
    //

    PALLOCATOR BitmapTableAllocator;
    PALLOCATOR HistogramTableAllocator;
    PALLOCATOR WordTableAllocator;
    PALLOCATOR LengthTableAllocator;

    //
    // Capture current longest and all-time longest word entries via the stats
    // structure.
    //

    DICTIONARY_STATS Stats;

    //
    // A slim read/writer lock guarding the dictionary.
    //

    DICTIONARY_LOCK Lock;

    //
    // Top-level bitmap table.
    //

    BITMAP_TABLE BitmapTable;

    //
    // Top-level word length table.
    //

    LENGTH_TABLE LengthTable;

} DICTIONARY;
typedef DICTIONARY *PDICTIONARY;

//
// Function typedefs for the AVL table's compare, allocate and free routines.
//

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI AVL_TABLE_COMPARE_ROUTINE)(
    _In_ struct _RTL_AVL_TABLE *Table,
    _In_ PVOID FirstStruct,
    _In_ PVOID SecondStruct
    );
typedef AVL_TABLE_COMPARE_ROUTINE *PAVL_TABLE_COMPARE_ROUTINE;

typedef
PVOID
(NTAPI AVL_TABLE_ALLOCATE_ROUTINE)(
    _In_ struct _RTL_AVL_TABLE *Table,
    _In_ CLONG ByteSize
    );
typedef AVL_TABLE_ALLOCATE_ROUTINE *PAVL_TABLE_ALLOCATE_ROUTINE;

typedef
VOID
(NTAPI AVL_TABLE_FREE_ROUTINE)(
    _In_ struct _RTL_AVL_TABLE *Table,
    _In_ __drv_freesMem(Buffer) _Post_invalid_ PVOID Buffer
    );
typedef AVL_TABLE_FREE_ROUTINE *PAVL_TABLE_FREE_ROUTINE;

//
// Forward declarations for the bitmap and histogram AVL table functions.
//

AVL_TABLE_COMPARE_ROUTINE BitmapTableCompareRoutine;
AVL_TABLE_ALLOCATE_ROUTINE BitmapTableAllocateRoutine;
AVL_TABLE_FREE_ROUTINE BitmapTableFreeRoutine;

AVL_TABLE_COMPARE_ROUTINE HistogramTableCompareRoutine;
AVL_TABLE_ALLOCATE_ROUTINE HistogramTableAllocateRoutine;
AVL_TABLE_FREE_ROUTINE HistogramTableFreeRoutine;

AVL_TABLE_COMPARE_ROUTINE WordTableCompareRoutine;
AVL_TABLE_ALLOCATE_ROUTINE WordTableAllocateRoutine;
AVL_TABLE_FREE_ROUTINE WordTableFreeRoutine;

AVL_TABLE_COMPARE_ROUTINE LengthTableCompareRoutine;
AVL_TABLE_ALLOCATE_ROUTINE LengthTableAllocateRoutine;
AVL_TABLE_FREE_ROUTINE LengthTableFreeRoutine;

//
// TLS-related structures and functions.
//

typedef struct _DICTIONARY_CONTEXT {
    PDICTIONARY Dictionary;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    PHISTOGRAM_TABLE HistogramTable;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;
    PWORD_ENTRY WordEntry;
} DICTIONARY_CONTEXT;
typedef DICTIONARY_CONTEXT *PDICTIONARY_CONTEXT;

extern ULONG DictionaryTlsIndex;

//
// The PROCESS_ATTACH and PROCESS_ATTACH functions share the same signature.
//

typedef
_Check_return_
_Success_(return != 0)
(DICTIONARY_TLS_FUNCTION)(
    _In_    HMODULE     Module,
    _In_    DWORD       Reason,
    _In_    LPVOID      Reserved
    );
typedef DICTIONARY_TLS_FUNCTION *PDICTIONARY_TLS_FUNCTION;

DICTIONARY_TLS_FUNCTION DictionaryTlsProcessAttach;
DICTIONARY_TLS_FUNCTION DictionaryTlsProcessDetach;

//
// Define TLS Get/Set context functions.
//

typedef
_Check_return_
_Success_(return != 0)
BOOLEAN
(DICTIONARY_TLS_SET_CONTEXT)(
    _In_ struct _DICTIONARY_CONTEXT *Context
    );
typedef DICTIONARY_TLS_SET_CONTEXT *PDICTIONARY_TLS_SET_CONTEXT;

typedef
_Check_return_
_Success_(return != 0)
struct _DICTIONARY_CONTEXT *
(DICTIONARY_TLS_GET_CONTEXT)(
    VOID
    );
typedef DICTIONARY_TLS_GET_CONTEXT *PDICTIONARY_TLS_GET_CONTEXT;

extern DICTIONARY_TLS_SET_CONTEXT DictionaryTlsSetContext;
extern DICTIONARY_TLS_GET_CONTEXT DictionaryTlsGetContext;

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
