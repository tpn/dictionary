/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    Histogram.c

Abstract:

    This module implements functionality related to the histogram functionality
    used by the dictionary.  In order to support efficient identification of
    word anagrams, histograms are employed to capture the frequency each byte
    value occurs in a given input string.  If two histograms match, the words
    can be considered anagrams.

    Functions are provided for comparing a histogram to another and determining
    if it is less than, equal to or greater than the other one.

--*/

#include "stdafx.h"

RTL_GENERIC_COMPARE_RESULTS
NTAPI
CompareHistogramsAlignedAvx2(
    PCCHARACTER_HISTOGRAM Left,
    PCCHARACTER_HISTOGRAM Right
    )
/*++

Routine Description:

    Compares two histograms using AVX2 intrinsics.  Underlying buffers must be
    aligned.

Arguments:

    Left - Supplies the left histogram to compare.

    Right - Supplies the right histogram to compare.

Return Value:

    GenericLessThan, GenericEqual or GenericGreaterThan depending on the result
    of the comparison.

    N.B. If the AVX2 instruction set is not support on the current CPU, this
         method will raise an EXCEPTION_ILLEGAL_INSTRUCTION.

    N.B. If either of the Left or Right parameters are not aligned on a 32-byte
         boundary, an EXCEPTION_ACCESS_VIOLATION will be raised.

--*/
{
    BYTE Index;
    BYTE Length;
    LONG EqualMask;
    LONG GreaterThanMask;

    YMMWORD LeftYmm;
    YMMWORD RightYmm;

    YMMWORD EqualYmm;
    YMMWORD GreaterThanYmm;

    //
    // Loop through each histogram 32-bytes at a time and compare values using
    // AVX2 intrinsics.
    //

    Length = ARRAYSIZE(Left->Ymm);

    for (Index = 0; Index < Length; Index++) {

        LeftYmm = _mm256_load_si256(&Left->Ymm[Index]);
        RightYmm = _mm256_load_si256(&Right->Ymm[Index]);

        EqualYmm = _mm256_cmpeq_epi32(LeftYmm, RightYmm);
        EqualMask = _mm256_movemask_epi8(EqualYmm);

        if (EqualMask == -1) {

            //
            // These two 32-byte chunks are equal, continue the comparison.
            //

            continue;
        }

        //
        // We've found a non-equal chunk.  Determine if it's greater than or
        // less than and terminate the loop.
        //

        GreaterThanYmm = _mm256_cmpgt_epi32(LeftYmm, RightYmm);
        GreaterThanMask = _mm256_movemask_epi8(GreaterThanYmm);

        if (GreaterThanMask == -1) {
            return GenericGreaterThan;
        } else {
            return GenericLessThan;
        }
    }

    //
    // If we get here, the loop exhausted all values and everything was found
    // to be equal, so return GenericEqual.
    //

    return GenericEqual;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
