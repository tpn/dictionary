/*++

Copyright (c) 2016 Trent Nelson <trent@trent.me>

Module Name:

    HeapAllocator.h

Abstract:

    This is the main header file for the HeapAllocator component.  It defines
    structures and functions related to the implementation of the component.

--*/

#pragma once

#include "stdafx.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Default heap routines that use the kernel32 heap routines.
//

typedef
_Check_return_
_Success_(return != 0)
BOOL
(INITIALIZE_HEAP_ALLOCATOR)(
    _In_ PALLOCATOR Allocator
    );
typedef INITIALIZE_HEAP_ALLOCATOR *PINITIALIZE_HEAP_ALLOCATOR;

typedef
_Check_return_
_Success_(return != 0)
BOOL
(INITIALIZE_HEAP_ALLOCATOR_EX)(
    _In_ PALLOCATOR Allocator,
    _In_ DWORD HeapCreateOptions,
    _In_ SIZE_T InitialSize,
    _In_ SIZE_T MaximumSize
    );
typedef INITIALIZE_HEAP_ALLOCATOR_EX *PINITIALIZE_HEAP_ALLOCATOR_EX;

typedef
VOID
(DESTROY_HEAP_ALLOCATOR)(
    _In_opt_ PALLOCATOR Allocator
    );
typedef DESTROY_HEAP_ALLOCATOR *PDESTROY_HEAP_ALLOCATOR;
typedef DESTROY_HEAP_ALLOCATOR **PPDESTROY_HEAP_ALLOCATOR;

//
// Export symbols.
//

RTL_API MALLOC HeapAllocatorMalloc;
RTL_API CALLOC HeapAllocatorCalloc;
RTL_API TRY_MALLOC HeapAllocatorTryMalloc;
RTL_API TRY_CALLOC HeapAllocatorTryCalloc;
RTL_API MALLOC_WITH_TIMESTAMP HeapAllocatorMallocWithTimestamp;
RTL_API CALLOC_WITH_TIMESTAMP HeapAllocatorCallocWithTimestamp;
RTL_API TRY_MALLOC_WITH_TIMESTAMP HeapAllocatorTryMallocWithTimestamp;
RTL_API TRY_CALLOC_WITH_TIMESTAMP HeapAllocatorTryCallocWithTimestamp;
RTL_API REALLOC HeapAllocatorRealloc;
RTL_API FREE HeapAllocatorFree;
RTL_API FREE_POINTER HeapAllocatorFreePointer;
RTL_API INITIALIZE_ALLOCATOR HeapAllocatorInitialize;
RTL_API DESTROY_ALLOCATOR HeapAllocatorDestroy;
RTL_API INITIALIZE_HEAP_ALLOCATOR InitializeHeapAllocator;
RTL_API INITIALIZE_HEAP_ALLOCATOR_EX InitializeHeapAllocatorEx;
RTL_API DESTROY_HEAP_ALLOCATOR DestroyHeapAllocator;

//
// Inline functions.
//

FORCEINLINE
BOOLEAN
InitializeHeapAllocatorExInline(
    _In_ PALLOCATOR Allocator,
    _In_ DWORD HeapCreateOptions,
    _In_ SIZE_T InitialSize,
    _In_ SIZE_T MaximumSize
    )
{
    HANDLE HeapHandle;

    if (!Allocator) {
        return FALSE;
    }

    HeapHandle = HeapCreate(HeapCreateOptions, InitialSize, MaximumSize);
    if (!HeapHandle) {
        return FALSE;
    }

    InitializeAllocator(
        Allocator,
        Allocator,
        HeapAllocatorMalloc,
        HeapAllocatorCalloc,
        HeapAllocatorRealloc,
        HeapAllocatorFree,
        HeapAllocatorFreePointer,
        HeapAllocatorInitialize,
        HeapAllocatorDestroy,
        HeapAllocatorTryMalloc,
        HeapAllocatorTryCalloc,
        HeapAllocatorMallocWithTimestamp,
        HeapAllocatorCallocWithTimestamp,
        HeapAllocatorTryMallocWithTimestamp,
        HeapAllocatorTryCallocWithTimestamp,
        HeapHandle
    );

    return TRUE;
}

FORCEINLINE
BOOLEAN
InitializeHeapAllocatorInline(
    _In_ PALLOCATOR Allocator
    )
{
    return InitializeHeapAllocatorExInline(Allocator, 0, 0, 0);
}

FORCEINLINE
BOOLEAN
InitializeNonSerializedHeapAllocatorInline(
    _In_ PALLOCATOR Allocator
    )
{
    return InitializeHeapAllocatorExInline(Allocator, HEAP_NO_SERIALIZE, 0, 0);
}

FORCEINLINE
VOID
DestroyHeapAllocatorInline(
    _In_ PALLOCATOR Allocator
    )
{
    if (Allocator->HeapHandle) {
        HeapDestroy(Allocator->HeapHandle);
    }

    return;
}


#ifdef __cplusplus
} // extern "C"
#endif

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
