/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Tables.c

Abstract:

    This module implements functionality related to the AVL table of character
    histograms used by the dictionary component.  This table forms the 2nd level
    hierarchy of word entries.

--*/

#include "stdafx.h"

//
// Implementations of the standard AVL table callback routines for comparison,
// allocation and freeing.
//

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
GenericTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct,
    PAVL_TABLE_COMPARE_ROUTINE ConfirmGenericEqualRoutine
    )
/*++

Routine Description:

    This routine is the generic table compare routine t
    AVL table mechanics during search and insert operations.

Arguments:

    Table - Supplies a pointer to the RTL_AVL_TABLE structure being used for
        the comparison.

    FirstStruct - Supplies a pointer to the first table entry structure to be
        used in the comparison.

    SecondStruct - Supplies a pointer to the second table entry structure to be
        used in the comparison.

    ConfirmGenericEqualRoutine - Optionally supplies a pointer to a routine
        that will be called if the initial comparison indicates that the values
        are equal.  This is used to perform a more expensive identity check
        once we've established that two hash values match, for example.

Return Value:

    An RTL_GENERIC_COMPARE_RESULTS value for the comparison: GenericLessThan,
    GenericEqual, or GenericGreaterThan.

--*/
{
    SHORT Offset = sizeof(RTL_BALANCED_LINKS);
    RTL_GENERIC_COMPARE_RESULTS Result;

    PTABLE_ENTRY_FULL First;
    PTABLE_ENTRY_FULL Second;

    ULONG FirstValue;
    ULONG SecondValue;

    //
    // Cast input parameters to the appropriate types.
    //

    First = (PTABLE_ENTRY_FULL)RtlOffsetToPointer(FirstStruct, -Offset);
    Second = (PTABLE_ENTRY_FULL)RtlOffsetToPointer(SecondStruct, -Offset);

    FirstValue = First->Value;
    SecondValue = Second->Value;

    if (FirstValue == SecondValue) {

        if (!ConfirmGenericEqualRoutine) {

            Result = GenericEqual;

        } else {

            Result = ConfirmGenericEqualRoutine(Table,
                                                FirstStruct,
                                                SecondStruct);

        }

    } else if (FirstValue < SecondValue) {

        Result = GenericLessThan;

    } else {

        Result = GenericGreaterThan;
    }

    return Result;
}

_Use_decl_annotations_
PVOID
NTAPI
GenericTableAllocateRoutine(
    PRTL_AVL_TABLE Table,
    CLONG ByteSize,
    PALLOCATOR Allocator
    )
/*++

Routine Description:

    This routine is the generic table entry allocation callback invoked by the
    AVL table mechanics to allocate memory for a new node during insert.

Arguments:

    Table - Supplies a pointer to the RTL_AVL_TABLE structure being used for
        the comparison.

    ByteSize - Number of bytes to allocate.  This will include the table header
        space for the RTL_BALANCED_LINKS structure.

    Allocator - Supplies a pointer to the ALLOCATOR structure to use for the
        memory allocation.

Return Value:

    Address of allocated memory if successful, NULL otherwise.

--*/
{
    return Allocator->Malloc(Allocator, ByteSize);
}

_Use_decl_annotations_
VOID
NTAPI
GenericTableFreeRoutine(
    PRTL_AVL_TABLE Table,
    PVOID Buffer,
    PALLOCATOR Allocator
    )
/*++

Routine Description:

    This routine is the generic table entry free callback invoked by the AVL
    table mechanics when an previously allocated node is being deleted (due
    to removal from the tree).

Arguments:

    Table - Supplies a pointer to the RTL_AVL_TABLE structure being used for
        the comparison.  This can be converted to the HISTOGRAM_TABLE by either
        CONTAINING_RECORD() or via the Table->TableContext field.

    Buffer - Supplies the address of the previously allocated memory to free.

    Allocator - Supplies a pointer to the ALLOCATOR structure to use to free
        the memory.  Must be the same one as used by the allocation routine.

Return Value:

    None.

--*/
{
    Allocator->Free(Allocator, Buffer);
    return;
}

//
// Table-specific confirmation routines for bitmaps and histograms.
//


RTL_GENERIC_COMPARE_RESULTS
NTAPI
BitmapConfirmEqualRoutine(
    PRTL_AVL_TABLE Table,
    PBITMAP_TABLE_ENTRY First,
    PBITMAP_TABLE_ENTRY Second
    )
{
    ULONG EqualMask;
    ULONG GreaterThanMask;

    YMMWORD FirstYmm;
    YMMWORD SecondYmm;

    YMMWORD EqualYmm;
    YMMWORD GreaterThanYmm;

    FirstYmm = _mm256_loadu_si256(&First->Bitmap.Ymm);
    SecondYmm = _mm256_loadu_si256(&Second->Bitmap.Ymm);

    EqualYmm = _mm256_cmpeq_epi32(FirstYmm, SecondYmm);
    GreaterThanYmm = _mm256_cmpgt_epi32(FirstYmm, SecondYmm);

    EqualMask = _mm256_movemask_epi8(EqualYmm);
    GreaterThanMask = _mm256_movemask_epi8(GreaterThanYmm);

    if (EqualMask == -1) {
        return GenericEqual;
    } else if (GreaterThanMask == -1) {
        return GenericGreaterThan;
    } else {
        return GenericLessThan;
    }

}

RTL_GENERIC_COMPARE_RESULTS
NTAPI
HistogramConfirmEqualRoutine(
    PRTL_AVL_TABLE Table,
    PHISTOGRAM_TABLE_ENTRY First,
    PHISTOGRAM_TABLE_ENTRY Second
    )
{
    BYTE Index;
    BYTE Length;
    LONG EqualMask;
    LONG GreaterThanMask;

    YMMWORD FirstYmm;
    YMMWORD SecondYmm;

    YMMWORD EqualYmm;
    YMMWORD GreaterThanYmm;

    Length = ARRAYSIZE(First->Histogram.Ymm);

    for (Index = 0; Index < Length; Index++) {

        FirstYmm = _mm256_loadu_si256(&First->Histogram.Ymm[Index]);
        SecondYmm = _mm256_loadu_si256(&Second->Histogram.Ymm[Index]);

        EqualYmm = _mm256_cmpeq_epi32(FirstYmm, SecondYmm);
        EqualMask = _mm256_movemask_epi8(EqualYmm);

        if (EqualMask == -1) {
            continue;
        }

        GreaterThanYmm = _mm256_cmpgt_epi32(FirstYmm, SecondYmm);
        GreaterThanMask = _mm256_movemask_epi8(GreaterThanYmm);

        if (GreaterThanMask == -1) {
            return GenericGreaterThan;
        } else {
            return GenericLessThan;
        }
    }

    return GenericEqual;
}

RTL_GENERIC_COMPARE_RESULTS
NTAPI
WordConfirmEqualRoutine(
    PRTL_AVL_TABLE Table,
    PWORD_TABLE_ENTRY_FULL First,
    PWORD_TABLE_ENTRY_FULL Second
    )
{
    __debugbreak();
    return GenericEqual;
}



//
// Table-specific entry points for the three AVL routines.
//


//
// Compare routines.
//

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
BitmapTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    )
{
    PAVL_TABLE_COMPARE_ROUTINE ConfirmEqual;

    ConfirmEqual = (PAVL_TABLE_COMPARE_ROUTINE)BitmapConfirmEqualRoutine;

    return GenericTableCompareRoutine(Table,
                                      FirstStruct,
                                      SecondStruct,
                                      ConfirmEqual);
}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
HistogramTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    )
{
    PAVL_TABLE_COMPARE_ROUTINE ConfirmEqual;

    ConfirmEqual = (PAVL_TABLE_COMPARE_ROUTINE)HistogramConfirmEqualRoutine;

    return GenericTableCompareRoutine(Table,
                                      FirstStruct,
                                      SecondStruct,
                                      ConfirmEqual);
}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
WordTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    )
{
    PAVL_TABLE_COMPARE_ROUTINE ConfirmEqual;

    ConfirmEqual = (PAVL_TABLE_COMPARE_ROUTINE)WordConfirmEqualRoutine;

    return GenericTableCompareRoutine(Table,
                                      FirstStruct,
                                      SecondStruct,
                                      ConfirmEqual);
}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
LengthTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    )
{
    return GenericTableCompareRoutine(Table,
                                      FirstStruct,
                                      SecondStruct,
                                      NULL);
}

//
// Allocate routines.
//

#define TABLE_TO_ALLOCATOR(Name) \
    (PALLOCATOR)(((PDICTIONARY)Table->TableContext)->Name##TableAllocator)

_Use_decl_annotations_
PVOID
NTAPI
BitmapTableAllocateRoutine(
    PRTL_AVL_TABLE Table,
    CLONG ByteSize
    )
{
    return GenericTableAllocateRoutine(Table,
                                       ByteSize,
                                       TABLE_TO_ALLOCATOR(Bitmap));
}

_Use_decl_annotations_
PVOID
NTAPI
HistogramTableAllocateRoutine(
    PRTL_AVL_TABLE Table,
    CLONG ByteSize
    )
{
    return GenericTableAllocateRoutine(Table,
                                       ByteSize,
                                       TABLE_TO_ALLOCATOR(Histogram));
}

_Use_decl_annotations_
PVOID
NTAPI
WordTableAllocateRoutine(
    PRTL_AVL_TABLE Table,
    CLONG ByteSize
    )
{
    return GenericTableAllocateRoutine(Table,
                                       ByteSize,
                                       TABLE_TO_ALLOCATOR(Word));
}

_Use_decl_annotations_
PVOID
NTAPI
LengthTableAllocateRoutine(
    PRTL_AVL_TABLE Table,
    CLONG ByteSize
    )
{
    return GenericTableAllocateRoutine(Table,
                                       ByteSize,
                                       TABLE_TO_ALLOCATOR(Length));
}

//
// Free routines.
//

_Use_decl_annotations_
VOID
NTAPI
BitmapTableFreeRoutine(
    PRTL_AVL_TABLE Table,
    PVOID Buffer
    )
{
    GenericTableFreeRoutine(Table, Buffer, TABLE_TO_ALLOCATOR(Bitmap));
}

_Use_decl_annotations_
VOID
NTAPI
HistogramTableFreeRoutine(
    PRTL_AVL_TABLE Table,
    PVOID Buffer
    )
{
    GenericTableFreeRoutine(Table, Buffer, TABLE_TO_ALLOCATOR(Histogram));
}

_Use_decl_annotations_
VOID
NTAPI
WordTableFreeRoutine(
    PRTL_AVL_TABLE Table,
    PVOID Buffer
    )
{
    GenericTableFreeRoutine(Table, Buffer, TABLE_TO_ALLOCATOR(Word));
}

_Use_decl_annotations_
VOID
NTAPI
LengthTableFreeRoutine(
    PRTL_AVL_TABLE Table,
    PVOID Buffer
    )
{
    GenericTableFreeRoutine(Table, Buffer, TABLE_TO_ALLOCATOR(Length));
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
