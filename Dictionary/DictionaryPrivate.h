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
#define NUMBER_OF_CHARACTER_BITS_IN_BYTES (256 / 8)                 // 32
#define NUMBER_OF_CHARACTER_BITS_IN_DOUBLEWORDS (256 / (4 << 3))    // 8
#define NUMBER_OF_CHARACTER_BITS_IN_QUADWORDS (256 / (8 << 3))      // 4

typedef union DECLSPEC_ALIGN(32) _CHARACTER_BITMAP {
     YMMWORD Ymm;
     XMMWORD Xmm[2];
     LONG Bits[NUMBER_OF_CHARACTER_BITS_IN_DOUBLEWORDS];
} CHARACTER_BITMAP;
C_ASSERT(sizeof(CHARACTER_BITMAP) == 32);
typedef CHARACTER_BITMAP *PCHARACTER_BITMAP;
typedef const CHARACTER_BITMAP *PCCHARACTER_BITMAP;

typedef union DECLSPEC_ALIGN(32) _CHARACTER_HISTOGRAM {
    YMMWORD Ymm[32];
    XMMWORD Xmm[64];
    ULONG Counts[NUMBER_OF_CHARACTER_BITS];
} CHARACTER_HISTOGRAM;
C_ASSERT(sizeof(CHARACTER_HISTOGRAM) == 1024);
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
INITIALIZE_WORD InitializeWord;

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI COMPARE_WORDS)(
    _In_ _Const_ PCLONG_STRING LeftString,
    _In_ _Const_ PCLONG_STRING RightString
    );
typedef COMPARE_WORDS *PCOMPARE_WORDS;
COMPARE_WORDS CompareWords;

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI COMPARE_CHARACTER_HISTOGRAMS)(
    _In_ _Const_ PCCHARACTER_HISTOGRAM Left,
    _In_ _Const_ PCCHARACTER_HISTOGRAM Right
    );
typedef COMPARE_CHARACTER_HISTOGRAMS *PCOMPARE_CHARACTER_HISTOGRAMS;

COMPARE_CHARACTER_HISTOGRAMS CompareHistogramsAlignedAvx2;

typedef
RTL_GENERIC_COMPARE_RESULTS
(NTAPI CONFIRM_GENERIC_EQUAL)(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    );


//
// Define the RTL_AVL_TABLE structure.  This mirrors the layout of the structure
// defined by the NT kernel, however, we utilize two unused ULONGs for our own
// purposes (tracking information about the table such as bytes allocated).
//

typedef struct _RTL_AVL_TABLE {
    RTL_BALANCED_LINKS BalancedRoot;
    PVOID OrderedPointer;
    ULONG WhichOrderedElement;
    ULONG NumberGenericTableElements;
    ULONG DepthOfTree;

    union {
        ULONG Unused1;
        ULONG BytesAllocatedLowPart;
    };

    PRTL_BALANCED_LINKS RestartKey;
    ULONG DeleteCount;

    union {
        ULONG Unused2;
        ULONG BytesAllocatedHighPart;
    };

    PRTL_AVL_COMPARE_ROUTINE CompareRoutine;
    PRTL_AVL_ALLOCATE_ROUTINE AllocateRoutine;
    PRTL_AVL_FREE_ROUTINE FreeRoutine;

    //
    // Our AVL tables always use the dictionary as the table context.
    //

    union {
        PVOID TableContext;
        PDICTIONARY Dictionary;
    };
} RTL_AVL_TABLE, *PRTL_AVL_TABLE;
C_ASSERT(FIELD_OFFSET(RTL_AVL_TABLE, OrderedPointer) == 32);
C_ASSERT(sizeof(RTL_AVL_TABLE) == 32+8+4+4+4+4+8+4+4+8+8+8+8);
C_ASSERT(sizeof(RTL_AVL_TABLE) == 104);

//
// Define a helper macro to for table entry enumeration.
//

#define FOR_EACH_ENTRY_IN_TABLE(Name, Type)                                 \
    for (Name##TableEntry = (Type)EnumerateTable(&Name##Table->Avl, TRUE);  \
         Name##TableEntry != NULL;                                          \
         Name##TableEntry = (Type)EnumerateTable(&Name##Table->Avl, FALSE))


//
// Define the length table.  We need to track word lengths in an ordered data
// structure in order to satisfy the constraint that if the current longest
// word is removed from the dictionary, the next longest word should be then
// promoted to the longest.
//

typedef struct _LENGTH_TABLE {
    RTL_AVL_TABLE Avl;
} LENGTH_TABLE;
typedef LENGTH_TABLE *PLENGTH_TABLE;

typedef struct _LENGTH_TABLE_ENTRY {
    LIST_ENTRY LengthListHead;
} LENGTH_TABLE_ENTRY;
typedef LENGTH_TABLE_ENTRY *PLENGTH_TABLE_ENTRY;


//
// Define the word table.  This is the third and final tier of the dictionary's
// data structure hierarchy.  Each entry is keyed by the underlying LONG_STRING
// representing the given word.
//

typedef struct _WORD_TABLE {
    RTL_AVL_TABLE Avl;
} WORD_TABLE;
typedef WORD_TABLE *PWORD_TABLE;

typedef struct _WORD_TABLE_ENTRY {
    WORD_ENTRY WordEntry;
    LIST_ENTRY LengthListEntry;
} WORD_TABLE_ENTRY;
typedef WORD_TABLE_ENTRY *PWORD_TABLE_ENTRY;

//
// Define the histogram table.  This is the second tier of the dictionary's
// data structure hierarchy.  Each entry is keyed by the 32-bit hash of the
// underlying histogram.  Thus, there could be collisions where the same hash
// value points to more than one histogram.  This is acceptable; when a word
// is queried for anagrams, we can verify histograms at that stage and omit
// words that don't match.  This trades extra CPU cost for space savings; the
// CHARACTER_HISTOGRAM structure is essentially an array of 256 ULONGs with a
// 32-byte alignment requirement.  Persisting this information for each entry
// would require an additional 1056 bytes.
//

typedef struct _HISTOGRAM_TABLE {
    RTL_AVL_TABLE Avl;
} HISTOGRAM_TABLE;
typedef HISTOGRAM_TABLE *PHISTOGRAM_TABLE;

//
// Each histogram table entry embeds another AVL table of word entries.
//

typedef struct _HISTOGRAM_TABLE_ENTRY {
    WORD_TABLE WordTable;
} HISTOGRAM_TABLE_ENTRY;
typedef HISTOGRAM_TABLE_ENTRY *PHISTOGRAM_TABLE_ENTRY;

//
// Define the bitmap table.  Each bitmap table entry embeds another AVL table
// of histogram entries.
//

typedef struct _BITMAP_TABLE {
    RTL_AVL_TABLE Avl;
} BITMAP_TABLE;
typedef BITMAP_TABLE *PBITMAP_TABLE;

typedef struct _BITMAP_TABLE_ENTRY {
    HISTOGRAM_TABLE HistogramTable;
} BITMAP_TABLE_ENTRY;
typedef BITMAP_TABLE_ENTRY *PBITMAP_TABLE_ENTRY;

//
// Define a generic table entry structure.  This mimics the layout of the table
// entry header (essentially, a RTL_BALANCED_LINKS structure) used by the AVL
// table routines.
//

typedef struct _TABLE_ENTRY_HEADER {

    union {

        //
        // Inline RTL_BALANCED_LINKS structure and abuse the ULONG at the end
        // of the structure normally used for padding for our own purposes.
        //

        struct {

            struct _RTL_BALANCED_LINKS *Parent;
            struct _RTL_BALANCED_LINKS *LeftChild;
            struct _RTL_BALANCED_LINKS *RightChild;

            union {

                struct {
                    CHAR Balance;
                    UCHAR Reserved[3];
                };

                struct {
                    ULONG BalanceBits:8;
                    ULONG ReservedBits:24;
                };

            };

            //
            // For bitmaps, histograms and word table entries, we stash the
            // 32-bit CRC32 of the data in the following field.  For length
            // entries, the length is stored.
            //

            union {
                ULONG Hash;
                ULONG Value;
                ULONG Length;
            };
        };

        RTL_BALANCED_LINKS BalancedLinks;

        //
        // Include RTL_SPLAY_LINKS which has the same pointer layout as the
        // start of RTL_BALANCED_LINKS, which allows us to use the predecessor
        // and successor Rtl functions.
        //

        RTL_SPLAY_LINKS SplayLinks;
    };

    //
    // The AVL routines will position our node-specific data at the offset
    // represented by this next field.  UserData essentially represents the
    // first 8 bytes of our custom table entry node data.  It is cast directly
    // to the various table entry subtypes (for bitmaps, histograms etc).
    //

    union {
        ULONGLONG UserData;
        struct _WORD_TABLE_ENTRY WordTableEntry;
        struct _LENGTH_TABLE_ENTRY LengthTableEntry;
        struct _BITMAP_TABLE_ENTRY BitmapTableEntry;
        struct _HISTOGRAM_TABLE_ENTRY HistogramTableEntry;
    };

} TABLE_ENTRY_HEADER;
typedef TABLE_ENTRY_HEADER *PTABLE_ENTRY_HEADER;
C_ASSERT(FIELD_OFFSET(TABLE_ENTRY_HEADER, UserData) == 32);

//
// Provide convenience macros for casting back and forth between table entry
// headers and the underlying entries.  Particularly useful in the AVL table
// comparison routine callbacks where we'll typically want to access the hash
// values embedded in the header.
//

#define TABLE_ENTRY_TO_HEADER(Entry)                             \
    ((PTABLE_ENTRY_HEADER)(                                      \
        RtlOffsetToPointer(                                      \
            Entry,                                               \
            -((SHORT)FIELD_OFFSET(TABLE_ENTRY_HEADER, UserData)) \
        )                                                        \
    ))

//
// Define the anagram word list structure used to link anagrams together.
// This is identical to the LINKED_WORD_LIST public structure with the addition
// of a bitmap and histogram plus respective hashes at the end of the structure.
//


typedef struct _ANAGRAM_LIST {

    union {

        //
        // Inline LINKED_WORD_LIST structure.
        //

        struct {
            LONGLONG NumberOfEntries;
            LIST_ENTRY ListHead;
        };

        LINKED_WORD_LIST LinkedWordList;
    };

    //
    // Pointer to the word table entry for which anagrams are being collected.
    //

    PWORD_TABLE_ENTRY WordTableEntry;

    //
    // Hashes for the bitmap and histogram.
    //

    ULONG BitmapHash;
    ULONG HistogramHash;

    //
    // The actual bitmap and histogram for this word table entry.
    //

    CHARACTER_BITMAP Bitmap;

    CHARACTER_HISTOGRAM Histogram;

} ANAGRAM_LIST;
typedef ANAGRAM_LIST *PANAGRAM_LIST;


//
// The default dictionary minimum and maximum word lengths are 1 byte and
// 1MB, respectively.  There's an absolute maximum word length of 16MB, which
// has to do with the fact that our histogram character count is limited to
// 24-bits during hashing (as the 8-bit index forms the rest of the ULONG).
// See comment below regarding the HASH structure for more information.
//
// Technically, this could be relaxed, the only impact would be the potential
// for more hash collisions.
//

#define MINIMUM_WORD_LENGTH          (1      )  //  1 byte
#define MAXIMUM_WORD_LENGTH          (1 << 20)  //  1 MB (1048576 bytes)
#define ABSOLUTE_MAXIMUM_WORD_LENGTH (1 << 24)  // 16 MB (16777216 bytes)

//
// Define the helper union used for capturing index and value as a 32-bit
// representation that can be passed into our hashing function.  As bitmaps
// and histograms will frequently have values of 0 (because the character has
// never been seen), including the ordinal of the character in the value being
// hashed ensures we're not hashing 0 over and over (which in the case of our
// currently implementation that uses CRC32, would have no effect on the hash).
//

typedef union _HASH {
    struct {
        ULONG Value:24;
        ULONG Index:8;
    };

    LONG AsLong;
    ULONG AsULong;
} HASH;
typedef HASH *PHASH;

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
    // Counters to track any length or histogram collisions.  Currently only
    // used for information and debugging purposes.  Set by GetWordAnagrams().
    //

    ULONG LengthCollisions;
    ULONG HistogramCollisions;

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
    // Pointer to word allocator (used for word copy operations).
    //

    PALLOCATOR WordAllocator;

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
    PVOID TableEntry;
    PTABLE_ENTRY_HEADER TableEntryHeader;
    PWORD_TABLE WordTable;
    PWORD_ENTRY WordEntry;
    PCLONG_STRING String;
    PWORD_TABLE_ENTRY WordTableEntry;
    PHISTOGRAM_TABLE HistogramTable;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;
} DICTIONARY_CONTEXT;
typedef DICTIONARY_CONTEXT *PDICTIONARY_CONTEXT;

extern ULONG DictionaryTlsIndex;

//
// Function typedefs for private functions.
//

typedef
_Success_(return != 0)
_Requires_lock_held_(Dictionary->Lock)
BOOLEAN
(NTAPI FIND_WORD_TABLE_ENTRY)(
    _In_ PDICTIONARY Dictionary,
    _In_z_ PCBYTE Word,
    _Out_writes_all_(sizeof(*Bitmap)) PCHARACTER_BITMAP Bitmap,
    _Out_writes_all_(sizeof(*Histogram)) PCHARACTER_HISTOGRAM Histogram,
    _Outptr_result_nullonfailure_ PWORD_TABLE_ENTRY *WordTableEntryPointer
    );
typedef FIND_WORD_TABLE_ENTRY *PFIND_WORD_TABLE_ENTRY;
extern FIND_WORD_TABLE_ENTRY FindWordTableEntry;

typedef
_Success_(return != 0)
_Requires_exclusive_lock_held_(Dictionary->Lock)
BOOLEAN
(NTAPI ADD_WORD_ENTRY)(
    _Inout_ PDICTIONARY Dictionary,
    _In_z_ PCBYTE Word,
    _Outptr_result_nullonfailure_ PCWORD_ENTRY *WordEntryPointer,
    _Outptr_ LONGLONG *EntryCountPointer
    );
typedef ADD_WORD_ENTRY *PADD_WORD_ENTRY;
extern ADD_WORD_ENTRY AddWordEntry;

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
