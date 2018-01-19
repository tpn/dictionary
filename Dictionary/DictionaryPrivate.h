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

    //
    // Embed the RTL_AVL_TABLE at the start of the structure.
    //

    _Guarded_by_(Lock)
    RTL_AVL_TABLE AvlTable;

    //
    // Define the lock for the structure.
    //

    DICTIONARY_LOCK Lock;

} HISTOGRAM_TABLE;
typedef HISTOGRAM_TABLE *PHISTOGRAM_TABLE;

typedef struct _HISTOGRAM_TABLE_ENTRY {

    //
    // A guarded linked-list of word entries for this histogram.  This equates
    // to a list of all anagrams for a given word entry.
    //

    GUARDED_LIST GuardedList;

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
            // Stash the hash for the bitmap here.
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

    _Guarded_by_(Lock)
    RTL_AVL_TABLE Avl;

    DICTIONARY_LOCK Lock;

} BITMAP_TABLE;
typedef BITMAP_TABLE *PBITMAP_TABLE;

typedef struct _BITMAP_TABLE_ENTRY {

    //
    // Histogram AVL table and associated lock.
    //

    _Guarded_by_(Lock)
    HISTOGRAM_TABLE HistogramTable;

    DICTIONARY_LOCK Lock;

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
    // Pointer to an initialized RTL structure.
    //

    PRTL Rtl;

    //
    // Pointer to an initialized ALLOCATOR structure.  This will be used to
    // satisfy allocation requests for AVL table entry nodes.
    //

    PALLOCATOR Allocator;

    //
    // A lock guarding the AVL table.
    //

    DICTIONARY_LOCK Lock;

    //
    // An AVL table used for capturing the first level character bitmap entry
    // nodes, keyed by the 32-bit hash of the bitmap.
    //

    _Guarded_by_(Lock)
    RTL_AVL_TABLE BitmapTable;

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
