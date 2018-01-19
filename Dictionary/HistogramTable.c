/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    HistogramTable.c

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
HistogramTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    )
/*++

Routine Description:

    This routine is the histogram table entry comparison callback invoked by the
    AVL table mechanics during search and insert operations.

Arguments:

    Table - Supplies a pointer to the RTL_AVL_TABLE structure being used for
        the comparison.  This can be converted to the HISTOGRAM_TABLE by either
        CONTAINING_RECORD() or via the Table->TableContext field.

    FirstStruct - Supplies a pointer to the first HISTOGRAM_TABLE_ENTRY structure
        to be used in the comparison.

    SecondStruct - Supplies a pointer to the second HISTOGRAM_TABLE_ENTRY structure
        to be used in the comparison.

Return Value:

    An RTL_GENERIC_COMPARE_RESULTS value for the comparison: GenericLessThan,
    GenericEqual, or GenericGreaterThan.

--*/
{
    SHORT Offset = sizeof(RTL_BALANCED_LINKS);
    RTL_GENERIC_COMPARE_RESULTS Result;
    PHISTOGRAM_TABLE HistogramTable;
    PHISTOGRAM_TABLE_ENTRY_FULL First;
    PHISTOGRAM_TABLE_ENTRY_FULL Second;
    ULONG FirstHash;
    ULONG SecondHash;

    //
    // Cast input parameters to the appropriate types.
    //

    HistogramTable = CONTAINING_RECORD(Table, HISTOGRAM_TABLE, AvlTable);

    First = (PHISTOGRAM_TABLE_ENTRY_FULL)RtlOffsetToPointer(FirstStruct,
                                                            -Offset);

    Second = (PHISTOGRAM_TABLE_ENTRY_FULL)RtlOffsetToPointer(SecondStruct,
                                                             -Offset);

    FirstHash = First->Hash;
    SecondHash = Second->Hash;

    if (FirstHash == SecondHash) {
        Result = GenericEqual;
    } else if (FirstHash < SecondHash) {
        Result = GenericLessThan;
    } else {
        Result = GenericGreaterThan;
    }

    return Result;
}

_Use_decl_annotations_
PVOID
NTAPI
HistogramTableAllocateRoutine(
    PRTL_AVL_TABLE Table,
    CLONG ByteSize
    )
/*++

Routine Description:

    This routine is the histogram table entry allocation callback invoked by the
    AVL table mechanics to allocate memory for a new node during insert.

Arguments:

    Table - Supplies a pointer to the RTL_AVL_TABLE structure being used for
        the comparison.  This can be converted to the HISTOGRAM_TABLE by either
        CONTAINING_RECORD() or via the Table->TableContext field.

    ByteSize - Number of bytes to allocate.  This will include the table header
        space for the RTL_BALANCED_LINKS structure.

Return Value:

    Address of allocated memory if successful, NULL otherwise.

--*/
{
    return NULL;
}

_Use_decl_annotations_
VOID
NTAPI
HistogramTableFreeRoutine(
    PRTL_AVL_TABLE Table,
    PVOID Buffer
    )
/*++

Routine Description:

    This routine is the histogram table entry free callback invoked by the AVL
    table mechanics when an previously allocated node is being deleted (due
    to removal from the tree).

Arguments:

    Table - Supplies a pointer to the RTL_AVL_TABLE structure being used for
        the comparison.  This can be converted to the HISTOGRAM_TABLE by either
        CONTAINING_RECORD() or via the Table->TableContext field.

    Buffer - Supplies the address of the previously allocated memory to free.

Return Value:

    None.

--*/
{
    return;
}


// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :