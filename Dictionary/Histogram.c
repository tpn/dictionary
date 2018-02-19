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

_Use_decl_annotations_
RTL_GENERIC_COMPARE_RESULTS
NTAPI
CompareHistogramsAlignedAvx2(
    PCCHARACTER_HISTOGRAM Left,
    PCCHARACTER_HISTOGRAM Right
    )
{
    return CompareHistogramsAlignedAvx2Inline(Left, Right);
}

_Use_decl_annotations_
BOOLEAN
NTAPI
CreateHistogram(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM Histogram
    )
{
    return CreateHistogramInline(String, Histogram);
}

_Use_decl_annotations_
BOOLEAN
NTAPI
CreateHistogramAvx2C(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM Histogram,
    PCHARACTER_HISTOGRAM TempHistogram
    )
{
    return CreateHistogramAvx2Inline(String, Histogram, TempHistogram);
}

_Use_decl_annotations_
BOOLEAN
NTAPI
CreateHistogramAvx2AlignedC(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM Histogram,
    PCHARACTER_HISTOGRAM TempHistogram
    )
{
    return CreateHistogramAvx2Inline(String, Histogram, TempHistogram);
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
