/*++

Copyright (c) 2017 Trent Nelson <trent@trent.me>

Module Name:

    HeapAllocator.c

Abstract:

    This module implements an ALLOCATOR interface around the standard Win32
    heap functions (HeapCreate(), HeapAlloc() etc).

--*/

#include "stdafx.h"

#define ContextToHeapHandle(Context) \
    (((PALLOCATOR)(Context))->HeapHandle)

_Use_decl_annotations_
PVOID
HeapAllocatorMalloc(
    PVOID Context,
    SIZE_T Size
    )
{
    return HeapAlloc(
        ContextToHeapHandle(Context),
        HEAP_ZERO_MEMORY,
        Size
    );
}

_Use_decl_annotations_
PVOID
HeapAllocatorCalloc(
    PVOID Context,
    SIZE_T NumberOfElements,
    SIZE_T SizeOfElements
    )
{
    SIZE_T Size = NumberOfElements * SizeOfElements;
    return HeapAlloc(
        ContextToHeapHandle(Context),
        HEAP_ZERO_MEMORY,
        Size
    );
}

_Use_decl_annotations_
PVOID
HeapAllocatorRealloc(
    PVOID Context,
    PVOID Buffer,
    SIZE_T NewSize
    )
{
    return HeapReAlloc(ContextToHeapHandle(Context), 0, Buffer, NewSize);
}

_Use_decl_annotations_
VOID
HeapAllocatorFree(
    PVOID Context,
    PVOID Buffer
    )
{
    HeapFree(ContextToHeapHandle(Context), 0, Buffer);
    return;
}

_Use_decl_annotations_
VOID
HeapAllocatorFreePointer(
    PVOID Context,
    PPVOID BufferPointer
    )
{
    if (!ARGUMENT_PRESENT(BufferPointer)) {
        return;
    }

    if (!ARGUMENT_PRESENT(*BufferPointer)) {
        return;
    }

    HeapAllocatorFree(Context, *BufferPointer);
    *BufferPointer = NULL;

    return;
}

_Use_decl_annotations_
VOID
HeapAllocatorDestroy(
    PALLOCATOR Allocator
    )
{
    if (!Allocator) {
        return;
    }

    if (Allocator->HeapHandle) {
        HeapDestroy(Allocator->HeapHandle);
        Allocator->HeapHandle = NULL;
    }

    return;
}

_Use_decl_annotations_
PVOID
HeapAllocatorTryMalloc(
    PVOID Context,
    SIZE_T Size
    )
{
    return HeapAllocatorMalloc(Context, Size);
}

_Use_decl_annotations_
PVOID
HeapAllocatorTryCalloc(
    PVOID Context,
    SIZE_T NumberOfElements,
    SIZE_T ElementSize
    )
{
    return HeapAllocatorCalloc(Context, NumberOfElements, ElementSize);
}

_Use_decl_annotations_
PVOID
HeapAllocatorMallocWithTimestamp(
    PVOID Context,
    SIZE_T Size,
    PLARGE_INTEGER TimestampPointer
    )
{
    UNREFERENCED_PARAMETER(TimestampPointer);
    return HeapAllocatorMalloc(Context, Size);
}

_Use_decl_annotations_
PVOID
HeapAllocatorCallocWithTimestamp(
    PVOID Context,
    SIZE_T NumberOfElements,
    SIZE_T ElementSize,
    PLARGE_INTEGER TimestampPointer
    )
{
    UNREFERENCED_PARAMETER(TimestampPointer);
    return HeapAllocatorCalloc(Context, NumberOfElements, ElementSize);
}

_Use_decl_annotations_
PVOID
HeapAllocatorTryMallocWithTimestamp(
    PVOID Context,
    SIZE_T Size,
    PLARGE_INTEGER TimestampPointer
    )
{
    UNREFERENCED_PARAMETER(TimestampPointer);
    return HeapAllocatorMalloc(Context, Size);
}

_Use_decl_annotations_
PVOID
HeapAllocatorTryCallocWithTimestamp(
    PVOID Context,
    SIZE_T NumberOfElements,
    SIZE_T ElementSize,
    PLARGE_INTEGER TimestampPointer
    )
{
    UNREFERENCED_PARAMETER(TimestampPointer);
    return HeapAllocatorCalloc(Context, NumberOfElements, ElementSize);
}

_Use_decl_annotations_
BOOLEAN
HeapAllocatorInitialize(
    PALLOCATOR Allocator
    )
{
    HANDLE HeapHandle;

    if (!Allocator) {
        return FALSE;
    }

    HeapHandle = HeapCreate(0, 0, 0);
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

_Use_decl_annotations_
BOOL
InitializeHeapAllocatorEx(
    PALLOCATOR Allocator,
    DWORD HeapCreateOptions,
    SIZE_T InitialSize,
    SIZE_T MaximumSize
    )
{
    return InitializeHeapAllocatorExInline(Allocator,
                                           HeapCreateOptions,
                                           InitialSize,
                                           MaximumSize);
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
