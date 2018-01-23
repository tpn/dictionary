/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Dictionary.c

Abstract:

    This module implements creation and initialization, and destruction routines
    for the dictionary component.  Additionally, it provides concrete function
    entry points for inline functions defined in public headers.

--*/

#include "stdafx.h"

_Use_decl_annotations_
BOOLEAN
CreateDictionary(
    PRTL Rtl,
    PALLOCATOR Allocator,
    DICTIONARY_CREATE_FLAGS CreateFlags,
    PDICTIONARY *DictionaryPointer
    )
/*++

Routine Description:

    Creates and initializes a DICTIONARY structure using the given allocator.

Arguments:

    Rtl - Supplies a pointer to an initialized RTL structure.  This is used to
        obtain function pointers to required NT kernel functions (such as those
        used for AVL tree manipulation).

    Allocator - Supplies a pointer to an initialized ALLOCATOR structure that
        will be used to allocate memory for the underlying DICTIONARY structure.

    CreateFlags - Optionally supplies creation flags that affect the underlying
        behavior of the dictionary.  Currently unused.

    DictionaryPointer - Supplies the address of a variable that will receive
        the address of the newly created DICTIONARY structure if the routine is
        successful (returns TRUE), or NULL if the routine failed.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BOOLEAN Success;
    PDICTIONARY Dictionary;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Rtl)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Allocator)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(DictionaryPointer)) {
        return FALSE;
    }

    //
    // CreateFlags aren't currently used.
    //

    if (CreateFlags.AsULong != 0) {
        return FALSE;
    }

    //
    // Clear the caller's pointer up-front.
    //

    *DictionaryPointer = NULL;

    //
    // Allocate space for the dictionary structure.
    //

    Dictionary = (PDICTIONARY)Allocator->Calloc(Allocator,
                                                1,
                                                sizeof(*Dictionary));

    if (!Dictionary) {
        goto Error;
    }

    //
    // Allocation was successful, continue with initialization.
    //

    Dictionary->SizeOfStruct = sizeof(*Dictionary);
    Dictionary->Rtl = Rtl;
    Dictionary->Allocator = Allocator;
    Dictionary->Flags.AsULong = 0;
    Dictionary->MinimumWordLength = MINIMUM_WORD_LENGTH;
    Dictionary->MaximumWordLength = MAXIMUM_WORD_LENGTH;

    //
    // Use the same allocator for all tables for now.
    //

    Dictionary->BitmapTableAllocator = Allocator;
    Dictionary->HistogramTableAllocator = Allocator;
    Dictionary->WordTableAllocator = Allocator;
    Dictionary->LengthTableAllocator = Allocator;

    //
    // Likewise for the word allocator.
    //

    Dictionary->WordAllocator = Allocator;

    //
    // Initialize the dictionary lock, acquire it exclusively, then initialize
    // the underlying AVL tables.  (We acquire and release it to satisfy the SAL
    // _Guarded_by_(Lock) annotation on the underlying BitmapTable element.)
    //

    InitializeDictionaryLock(&Dictionary->Lock);

    AcquireDictionaryLockExclusive(&Dictionary->Lock);

    Rtl->RtlInitializeGenericTableAvl(&Dictionary->BitmapTable.Avl,
                                      BitmapTableCompareRoutine,
                                      BitmapTableAllocateRoutine,
                                      BitmapTableFreeRoutine,
                                      Dictionary);

    Rtl->RtlInitializeGenericTableAvl(&Dictionary->LengthTable.Avl,
                                      LengthTableCompareRoutine,
                                      LengthTableAllocateRoutine,
                                      LengthTableFreeRoutine,
                                      Dictionary);


    ReleaseDictionaryLockExclusive(&Dictionary->Lock);

    //
    // We've completed initialization, indicate success and jump to the end.
    //

    Success = TRUE;

    goto End;

Error:

    Success = FALSE;

    //
    // Intentional follow-on to End.
    //

End:

    //
    // Update the caller's pointer and return.
    //
    // N.B. Dictionary could be NULL here, which is fine.
    //

    *DictionaryPointer = Dictionary;

    return Success;
}

_Use_decl_annotations_
BOOLEAN
DestroyDictionary(
    PDICTIONARY *DictionaryPointer,
    PBOOLEAN IsProcessTerminating
    )
/*++

Routine Description:

    Destroys a previously created DICTIONARY structure, freeing all memory
    unless the IsProcessTerminating flag is TRUE.

Arguments:

    DictionaryPointer - Supplies the address of a variable that contains the
        address of the DICTIONARY structure to destroy.  This variable will be
        cleared (i.e. the pointer will be set to NULL) if the routine destroys
        the dictionary successfully (returns TRUE).

    IsProcessTerminating - Optionally supplies a pointer to a boolean flag
        indicating whether or not the process is terminating.  If the pointer
        is non-NULL and the underlying value is TRUE, the method returns success
        immediately.  (If the process is terminating, there is no need to walk
        the dictionary and all sub-trees freeing each one individually.)

Return Value:

    TRUE on success, FALSE on failure.  If successful, a NULL pointer will be
    written to the DictionaryPointer parameter.

--*/
{
    PRTL Rtl;
    BOOLEAN Success;
    PALLOCATOR Allocator;
    PDICTIONARY Dictionary;
    PWORD_TABLE WordTable;
    PBITMAP_TABLE BitmapTable;
    PHISTOGRAM_TABLE HistogramTable;
    PWORD_TABLE_ENTRY WordTableEntry;
    PBITMAP_TABLE_ENTRY BitmapTableEntry;
    PHISTOGRAM_TABLE_ENTRY HistogramTableEntry;
    PRTL_ENUMERATE_GENERIC_TABLE_AVL EnumerateTable;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(DictionaryPointer)) {
        goto Error;
    }

    if (ARGUMENT_PRESENT(IsProcessTerminating)) {
        if (*IsProcessTerminating) {

            //
            // Fast-path exit.  Clear the caller's pointer and return success.
            //

            *DictionaryPointer = NULL;
            return TRUE;
        }
    }

    //
    // A valid pointer has been provided, and the process isn't terminating.
    // Initialize aliases and continue with destroy logic.
    //

    Dictionary = *DictionaryPointer;
    Rtl = Dictionary->Rtl;
    Allocator = Dictionary->Allocator;
    EnumerateTable = Rtl->RtlEnumerateGenericTableAvl;

    BitmapTable = &Dictionary->BitmapTable;
    //BitmapAvl = &BitmapTable->Avl;

    //
    // Sanity check the dictionary structure size matches what we expect.
    //

    ASSERT(Dictionary->SizeOfStruct == sizeof(*Dictionary));

    //
    // Acquire the dictionary lock for the duration of the destroy logic.
    //

    AcquireDictionaryLockExclusive(&Dictionary->Lock);


    FOR_EACH_ENTRY_IN_TABLE(Bitmap, PBITMAP_TABLE_ENTRY) {

        //
        // Resolve the histogram table for the current bitmap table entry.
        //

        HistogramTable = &BitmapTableEntry->HistogramTable;
        //HistogramAvl = HistogramTable->Avl;

        FOR_EACH_ENTRY_IN_TABLE(Histogram, PHISTOGRAM_TABLE_ENTRY) {

            //
            // Resolve the word table for the current histogram table entry.
            //

            WordTable = &HistogramTableEntry->WordTable;
            //WordAvl = &WordTable->Avl;

            FOR_EACH_ENTRY_IN_TABLE(Word, PWORD_TABLE_ENTRY) {

                NOTHING;

            }
        }
    }

    //
    // Now free the dictionary itself.
    //

    Allocator->FreePointer(Allocator, DictionaryPointer);

    Success = TRUE;

    goto End;

Error:

    Success = FALSE;

    //
    // Intentional follow-on to End.
    //

End:

    return Success;
}

_Use_decl_annotations_
BOOLEAN
GetDictionaryStats(
    PDICTIONARY Dictionary,
    PALLOCATOR Allocator,
    PDICTIONARY_STATS *DictionaryStatsPointer
    )
/*++

Routine Description:

    Gets the dictionary statistics.

Arguments:

    Dictionary - Supplies a pointer to the DICTIONARY structure for which the
        statistics are to be obtained.

    Allocator - Supplies a pointer to an ALLOCATOR structure that will be used
        to allocate memory for the underlying dictionary statistics.

    DictionaryStatsPointer - Supplies the address to a variable that will
        receive the address of the dictionary statistics structure.  The
        caller is responsible for freeing this address when finished via
        the Allocator parameter passed in above.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    PBYTE Buffer;
    LARGE_INTEGER AllocSize;
    PDICTIONARY_STATS Stats;
    PCLONG_STRING CurrentLongestWord;
    PCLONG_STRING LongestWordAllTime;
    PLONG_STRING NewCurrentLongestWord;
    PLONG_STRING NewLongestWordAllTime;
    ULONG AlignedCurrentLongestWordBufferSize;
    ULONG AlignedLongestWordAllTimeBufferSize;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Allocator)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(DictionaryStatsPointer)) {
        return FALSE;
    }

    //
    // Clear the caller's pointer up-front.
    //

    *DictionaryStatsPointer = NULL;

    //
    // Calculate allocation size required for the dictionary stats and all
    // supporting structures and string buffers.  We do this up front such
    // that we can dispatch a single allocation call to the allocator, which
    // allows us to return a pointer to the user that can be subsequently
    // freed with a single call, too.
    //

    AllocSize.QuadPart = (

        //
        // Account for the dictionary stats structure size..
        //

        sizeof(DICTIONARY_STATS)

    );

    //
    // Account for the current longest word.
    //

    CurrentLongestWord = Dictionary->Stats.CurrentLongestWord;

    if (CurrentLongestWord) {

        AlignedCurrentLongestWordBufferSize = (
            ALIGN_UP_POINTER(CurrentLongestWord->Length + 1)
        );

        AllocSize.QuadPart += (

            //
            // Account for the LONG_STRING structure size.
            //

            sizeof(LONG_STRING) +

            //
            // Account for the 8-byte aligned buffer size of the string,
            // including the terminating NULL.
            //

            AlignedCurrentLongestWordBufferSize
        );

    }

    //
    // Account for the longest word of all time.
    //

    LongestWordAllTime = Dictionary->Stats.LongestWordAllTime;

    if (LongestWordAllTime) {

        AlignedLongestWordAllTimeBufferSize = (
            ALIGN_UP_POINTER(LongestWordAllTime->Length + 1)
        );

        AllocSize.QuadPart += (

            //
            // Account for the LONG_STRING structure size.
            //

            sizeof(LONG_STRING) +

            //
            // Account for the 8-byte aligned buffer size of the string,
            // including the terminating NULL.
            //

            AlignedLongestWordAllTimeBufferSize
        );
    }

    //
    // Sanity check our allocation size isn't over ULONG.
    //

    ASSERT(AllocSize.HighPart == 0);

    //
    // Allocate space.
    //

    Buffer = (PBYTE)Allocator->Calloc(Allocator, 1, AllocSize.LowPart);

    if (!Buffer) {
        return FALSE;
    }

    //
    // Carve out the dictionary stats structure.
    //

    Stats = (PDICTIONARY_STATS)Buffer;
    Buffer += sizeof(DICTIONARY_STATS);

    if (CurrentLongestWord) {

        //
        // Carve out the underlying LONG_STRING structure.
        //

        NewCurrentLongestWord = (PLONG_STRING)Buffer;
        Buffer += sizeof(LONG_STRING);

        //
        // Carve out the underlying string buffer.
        //

        NewCurrentLongestWord->Buffer = Buffer;
        Buffer += AlignedCurrentLongestWordBufferSize;

        //
        // Copy the string buffer over.
        //

        CopyMemory(NewCurrentLongestWord->Buffer,
                   CurrentLongestWord->Buffer,
                   CurrentLongestWord->Length);

        //
        // Ensure the string is NULL terminated.  (Not technically necessary
        // as the Calloc() call to the allocator should have zero'd all the
        // memory for us.)
        //

        NewCurrentLongestWord->Buffer[CurrentLongestWord->Length] = '\0';

        //
        // Copy over the length and hash values.
        //

        NewCurrentLongestWord->Length = CurrentLongestWord->Length;
        NewCurrentLongestWord->Hash = CurrentLongestWord->Hash;

        //
        // Wire it up to our new stats structure.
        //

        Stats->CurrentLongestWord = (PCLONG_STRING)NewCurrentLongestWord;
    }

    //
    // Do the same for the longest all time word.
    //

    if (LongestWordAllTime) {

        //
        // Carve out the underlying LONG_STRING structure.
        //

        NewLongestWordAllTime = (PLONG_STRING)Buffer;
        Buffer += sizeof(LONG_STRING);

        //
        // Carve out the underlying string buffer.
        //

        NewLongestWordAllTime->Buffer = Buffer;
        Buffer += AlignedLongestWordAllTimeBufferSize;

        //
        // Copy the string buffer over.
        //

        CopyMemory(NewLongestWordAllTime->Buffer,
                   LongestWordAllTime->Buffer,
                   LongestWordAllTime->Length);

        //
        // Ensure the string is NULL terminated.  (Not technically necessary
        // as the Calloc() call to the allocator should have zero'd all the
        // memory for us.)
        //

        NewLongestWordAllTime->Buffer[LongestWordAllTime->Length] = '\0';

        //
        // Copy over the length and hash values.
        //

        NewLongestWordAllTime->Length = LongestWordAllTime->Length;
        NewLongestWordAllTime->Hash = LongestWordAllTime->Hash;

        //
        // Wire it up to our new stats structure.
        //

        Stats->LongestWordAllTime = (PCLONG_STRING)NewLongestWordAllTime;
    }

    //
    // Sanity check our buffer address.
    //

    ASSERT(Buffer = (PBYTE)RtlOffsetToPointer(Stats, AllocSize.LowPart));

    //
    // Update the caller's pointer and return success.
    //

    *DictionaryStatsPointer = Stats;

    return TRUE;
}

//
// Helper functions for setting minimum and maximum length values.
//

_Use_decl_annotations_
BOOLEAN
SetMinimumWordLength(
    PDICTIONARY Dictionary,
    ULONG MinimumWordLength
    )
{
    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!MinimumWordLength) {
        return FALSE;
    }

    if (MinimumWordLength > Dictionary->MaximumWordLength) {
        return FALSE;
    }

    if (MinimumWordLength > ABSOLUTE_MAXIMUM_WORD_LENGTH) {
        return FALSE;
    }

    //
    // Value has been validated; update the dictionary accordingly.
    //

    Dictionary->MinimumWordLength = MinimumWordLength;

    return TRUE;
}

_Use_decl_annotations_
BOOLEAN
SetMaximumWordLength(
    PDICTIONARY Dictionary,
    ULONG MaximumWordLength
    )
{
    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Dictionary)) {
        return FALSE;
    }

    if (!MaximumWordLength) {
        return FALSE;
    }

    if (MaximumWordLength < Dictionary->MinimumWordLength) {
        return FALSE;
    }

    if (MaximumWordLength > ABSOLUTE_MAXIMUM_WORD_LENGTH) {
        return FALSE;
    }

    //
    // Value has been validated; update the dictionary accordingly.
    //

    Dictionary->MaximumWordLength = MaximumWordLength;

    return TRUE;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
