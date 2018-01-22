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
    PVOID LeftEntry,
    PVOID RightEntry
    )
/*++

Routine Description:

    This routine is the generic table compare routine for AVL tables used by
    the dictionary.  It uses the spare ULONG embedded at the end of the struct
    RTL_BALANCED_LINKS for the basis of the comparison.

Arguments:

    Table - Supplies a pointer to the RTL_AVL_TABLE structure being used for
        the comparison.

    LeftEntry - Supplies a pointer to the left table entry header structure to
        be used in the comparison.

    RightEntry - Supplies a pointer to the right table entry header structure
        to be used in the comparison.

Return Value:

    An RTL_GENERIC_COMPARE_RESULTS value for the comparison: GenericLessThan,
    GenericEqual, or GenericGreaterThan.

--*/
{
    PTABLE_ENTRY_HEADER Left;
    PTABLE_ENTRY_HEADER Right;
    RTL_GENERIC_COMPARE_RESULTS Result;

    //
    // Convert the table entries to their respective headers.
    //

    Left = TABLE_ENTRY_TO_HEADER(LeftEntry);
    Right = TABLE_ENTRY_TO_HEADER(RightEntry);

    //
    // A value of 0 for either the left or the right is usually indicative
    // of an internal issue.
    //

    ASSERT(Left->Value != 0 && Right->Value != 0);

    if (Left->Value == Right->Value) {

        Result = GenericEqual;

    } else if (Left->Value < Right->Value) {

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
    return Allocator->Calloc(Allocator, 1, ByteSize);
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
    PVOID LeftEntry,
    PVOID RightEntry
    )
{
    return GenericTableCompareRoutine(Table, LeftEntry, RightEntry);
}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
HistogramTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID LeftEntry,
    PVOID RightEntry
    )
{
    return GenericTableCompareRoutine(Table, LeftEntry, RightEntry);
}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
WordTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID LeftEntry,
    PVOID RightEntry
    )
{
    PWORD_TABLE_ENTRY Left;
    PWORD_TABLE_ENTRY Right;
    PCLONG_STRING LeftString;
    PCLONG_STRING RightString;
    PTABLE_ENTRY_HEADER LeftHeader;
    PTABLE_ENTRY_HEADER RightHeader;
    PDICTIONARY_CONTEXT Context;

    RTL_GENERIC_COMPARE_RESULTS Result;

    //
    // Convert the table entries to their respective headers.
    //

    LeftHeader = TABLE_ENTRY_TO_HEADER(LeftEntry);
    RightHeader = TABLE_ENTRY_TO_HEADER(RightEntry);

    Context = DictionaryTlsGetContext();

    Result = GenericTableCompareRoutine(Table, LeftEntry, RightEntry);

    if (Result == GenericEqual) {

        //
        // The hash values of the string match; resolve the underlying word
        // entry structures and check the string lengths.  If they're also
        // equal, compare the byte values.
        //

        Left = (PWORD_TABLE_ENTRY)LeftEntry;
        Right = (PWORD_TABLE_ENTRY)RightEntry;

        LeftString = &Left->WordEntry.String;
        RightString = &Right->WordEntry.String;

        if (LeftString->Length == RightString->Length) {

            //
            // Lengths also match.  Dispatch to the byte comparison routine.
            //

            Result = CompareWords(LeftString, RightString);

        } else if (LeftString->Length < RightString->Length) {

            Result = GenericLessThan;

        } else {

            Result = GenericGreaterThan;
        }
    }

    return Result;
}

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
LengthTableCompareRoutine(
    PRTL_AVL_TABLE Table,
    PVOID LeftEntry,
    PVOID RightEntry
    )
{
    return GenericTableCompareRoutine(Table, LeftEntry, RightEntry);
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
