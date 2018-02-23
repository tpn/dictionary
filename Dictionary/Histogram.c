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
    BYTE Byte;
    ULONG Index;
    PBYTE Buffer;
    PULONG Counts;
    BOOLEAN Success;
    LONGLONG Remaining;
    PYMMWORD BufferYmm;
    ULONGLONG Alignment;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;

    YMMWORD Ymm0;
    YMMWORD Ymm1;
    YMMWORD Ymm2;
    YMMWORD Ymm3;

    //
    // Initialize aliases.
    //

    Buffer = String->Buffer;
    Remaining = (LONG)String->Length;
    Histogram1 = Histogram;
    Histogram2 = TempHistogram;
    Counts = (PULONG)&Histogram->Counts;

    //
    // Obtain the string buffer's alignment.
    //

    Alignment = GetAddressAlignment(Buffer);

    ASSERT(Alignment >= 32);
    ASSERT(Remaining >= 64);

    //
    // Initialize the YMM buffer alias.
    //

    BufferYmm = (PYMMWORD)Buffer;

    //
    // Attempt to process as many 64 byte chunks as possible.
    //

    while (Remaining >= 64) {

        //
        // We have at least 64 bytes remaining and our buffer's address alignment
        // is suitable for YMM loading.  Dispatch two loads into corresponding
        // YMM registers.
        //

        Ymm0 = Ymm1 = _mm256_load_si256(BufferYmm);
        Ymm2 = Ymm3 = _mm256_load_si256(BufferYmm+1);

        //
        // Subtract 64 bytes from our remaining byte count and advance our
        // buffer twice (to account for the 2 x 32-byte loads we just did).
        //

        Remaining -= 64;
        BufferYmm += 2;

        //
        // Unroll the histogram counting logic in chunks of 32 bytes.  Alternate
        // between Ymm0 and Ymm1 for the first 32 bytes, and Ymm2 and Ymm3 for
        // the second 32 bytes.
        //

        //
        // Bytes 0-31, registers Ymm0 and Ymm1.
        //

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  0)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  1)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  2)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  3)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  4)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  5)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  6)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  7)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  8)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  9)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 10)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 11)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 12)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 13)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 14)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 15)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 16)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 17)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 18)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 19)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 20)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 21)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 22)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 23)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 24)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 25)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 26)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 27)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 28)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 29)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 30)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 31)]++;

        //
        // Bytes 32-63, registers Ymm2 and Ymm3.
        //

        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  0)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  1)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  2)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  3)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  4)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  5)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  6)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  7)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  8)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  9)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 10)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 11)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 12)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 13)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 14)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 15)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 16)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 17)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 18)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 19)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 20)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 21)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 22)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 23)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 24)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 25)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 26)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 27)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 28)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 29)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 30)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 31)]++;

    }

    ASSERT(Remaining >= 0);

    if (Remaining > 0) {

        //
        // Advance the byte buffer pointer to its YMM counterpart
        // and manually calculate the histogram for trailing bytes.
        //

        Buffer = (PBYTE)BufferYmm;

        for (Index = 0; Index < (ULONG)Remaining; Index++) {
            Byte = Buffer[Index];
            Counts[Byte]++;
        }

    }

    //
    // Sum the temporary histogram (Histogram2) with the caller-provided
    // histogram (Histogram1).
    //

    for (Index = 0; Index < 32; Index++) {

        //
        // Load a 32-byte chunk of each histogram into YMM registers.
        //

        Ymm0 = _mm256_load_si256(&Histogram1->Ymm[Index]);
        Ymm1 = _mm256_load_si256(&Histogram2->Ymm[Index]);

        //
        // Add the two vectors.
        //

        Ymm2 = _mm256_add_epi32(Ymm0, Ymm1);

        //
        // Store the result back in the first histogram.
        //

        _mm256_store_si256(&Histogram1->Ymm[Index], Ymm2);
    }

    //
    // Indicate success and return.
    //

    Success = TRUE;

    return Success;
}

_Use_decl_annotations_
BOOLEAN
NTAPI
CreateHistogramAvx2AlignedCV4(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM_V4 Histogram
    )
{
    BYTE Byte;
    ULONG Index;
    PBYTE Buffer;
    PULONG Counts;
    BOOLEAN Success;
    LONGLONG Remaining;
    PYMMWORD BufferYmm;
    ULONGLONG Alignment;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;
    PCHARACTER_HISTOGRAM Histogram3;
    PCHARACTER_HISTOGRAM Histogram4;

    YMMWORD Ymm0;
    YMMWORD Ymm1;
    YMMWORD Ymm2;
    YMMWORD Ymm3;
    YMMWORD Ymm4;
    YMMWORD Ymm5;
    YMMWORD Ymm6;

    //
    // Initialize aliases.
    //

    Buffer = String->Buffer;
    Remaining = (LONG)String->Length;
    Histogram1 = &Histogram->Histogram1;
    Histogram2 = &Histogram->Histogram2;
    Histogram3 = &Histogram->Histogram3;
    Histogram4 = &Histogram->Histogram4;
    Counts = (PULONG)&Histogram1->Counts;

    //
    // Obtain the string buffer's alignment.
    //

    Alignment = GetAddressAlignment(Buffer);

    ASSERT(Alignment >= 32);
    ASSERT(Remaining >= 64);

    //
    // Initialize the YMM buffer alias.
    //

    BufferYmm = (PYMMWORD)Buffer;

    //
    // Attempt to process as many 64 byte chunks as possible.
    //

    while (Remaining >= 64) {

        //
        // We have at least 64 bytes remaining and our buffer's address alignment
        // is suitable for YMM loading.  Dispatch two loads into corresponding
        // YMM registers.
        //

        Ymm0 = Ymm1 = _mm256_load_si256(BufferYmm);
        Ymm2 = Ymm3 = _mm256_load_si256(BufferYmm+1);

        //
        // Subtract 64 bytes from our remaining byte count and advance our
        // buffer twice (to account for the 2 x 32-byte loads we just did).
        //

        Remaining -= 64;
        BufferYmm += 2;

        //
        // Unroll the histogram counting logic in chunks of 32 bytes.  Alternate
        // between Ymm0 and Ymm1 for the first 32 bytes, and Ymm2 and Ymm3 for
        // the second 32 bytes.
        //

        //
        // Bytes 0-31, registers Ymm0 and Ymm1.
        //

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  0)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  1)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0,  2)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1,  3)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  4)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  5)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0,  6)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1,  7)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  8)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  9)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0, 10)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1, 11)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 12)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 13)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0, 14)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1, 15)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 16)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 17)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0, 18)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1, 19)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 20)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 21)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0, 22)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1, 23)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 24)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 25)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0, 26)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1, 27)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 28)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 29)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm0, 30)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm1, 31)]++;

        //
        // Bytes 32-63, registers Ymm2 and Ymm3.
        //

        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  0)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  1)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2,  2)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3,  3)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  4)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  5)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2,  6)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3,  7)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2,  8)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3,  9)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2, 10)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3, 11)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 12)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 13)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2, 14)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3, 15)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 16)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 17)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2, 18)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3, 19)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 20)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 21)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2, 22)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3, 23)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 24)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 25)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2, 26)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3, 27)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm2, 28)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm3, 29)]++;
        Histogram3->Counts[_mm256_extract_epi8(Ymm2, 30)]++;
        Histogram4->Counts[_mm256_extract_epi8(Ymm3, 31)]++;

    }

    ASSERT(Remaining >= 0);

    if (Remaining > 0) {

        //
        // Advance the byte buffer pointer to its YMM counterpart
        // and manually calculate the histogram for trailing bytes.
        //

        Buffer = (PBYTE)BufferYmm;

        for (Index = 0; Index < (ULONG)Remaining; Index++) {
            Byte = Buffer[Index];
            Counts[Byte]++;
        }

    }

    //
    // Sum the temporary histogram (Histogram2) with the caller-provided
    // histogram (Histogram1).
    //

    for (Index = 0; Index < 32; Index++) {

        //
        // Load a 32-byte chunk of each histogram into YMM registers.
        //

        Ymm0 = _mm256_load_si256(&Histogram1->Ymm[Index]);
        Ymm1 = _mm256_load_si256(&Histogram2->Ymm[Index]);
        Ymm4 = _mm256_add_epi32(Ymm0, Ymm1);

        Ymm2 = _mm256_load_si256(&Histogram3->Ymm[Index]);
        Ymm3 = _mm256_load_si256(&Histogram4->Ymm[Index]);
        Ymm5 = _mm256_add_epi32(Ymm2, Ymm3);

        Ymm6 = _mm256_add_epi32(Ymm4, Ymm5);

        //
        // Store the result back in the first histogram.
        //

        _mm256_store_si256(&Histogram1->Ymm[Index], Ymm6);
    }

    //
    // Indicate success and return.
    //

    Success = TRUE;

    return Success;
}

_Use_decl_annotations_
BOOLEAN
NTAPI
CreateHistogramAvx2AlignedC32(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM Histogram,
    PCHARACTER_HISTOGRAM TempHistogram
    )
{
    BYTE Byte;
    ULONG Index;
    PBYTE Buffer;
    PULONG Counts;
    BOOLEAN Success;
    LONGLONG Remaining;
    PYMMWORD BufferYmm;
    ULONGLONG Alignment;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;

    YMMWORD Ymm0;
    YMMWORD Ymm1;
    YMMWORD Ymm2;

    //
    // Initialize aliases.
    //

    Buffer = String->Buffer;
    Remaining = (LONG)String->Length;
    Histogram1 = Histogram;
    Histogram2 = TempHistogram;
    Counts = (PULONG)&Histogram->Counts;

    //
    // Obtain the string buffer's alignment.
    //

    Alignment = GetAddressAlignment(Buffer);

    ASSERT(Alignment >= 32);
    ASSERT(Remaining >= 32);

    //
    // Initialize the YMM buffer alias.
    //

    BufferYmm = (PYMMWORD)Buffer;

    //
    // Attempt to process as many 32 byte chunks as possible.
    //

    while (Remaining >= 32) {


        Ymm0 = Ymm1 = _mm256_load_si256(BufferYmm);

        //
        // Subtract 64 bytes from our remaining byte count and advance our
        // buffer twice (to account for the 2 x 32-byte loads we just did).
        //

        Remaining -= 32;
        BufferYmm += 1;

        //
        // Bytes 0-31, registers Ymm0 and Ymm1.
        //

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  0)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  1)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  2)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  3)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  4)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  5)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  6)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  7)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0,  8)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1,  9)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 10)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 11)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 12)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 13)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 14)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 15)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 16)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 17)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 18)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 19)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 20)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 21)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 22)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 23)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 24)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 25)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 26)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 27)]++;

        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 28)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 29)]++;
        Histogram1->Counts[_mm256_extract_epi8(Ymm0, 30)]++;
        Histogram2->Counts[_mm256_extract_epi8(Ymm1, 31)]++;

    }

    ASSERT(Remaining >= 0);

    if (Remaining > 0) {

        //
        // Advance the byte buffer pointer to its YMM counterpart
        // and manually calculate the histogram for trailing bytes.
        //

        Buffer = (PBYTE)BufferYmm;

        for (Index = 0; Index < (ULONG)Remaining; Index++) {
            Byte = Buffer[Index];
            Counts[Byte]++;
        }

    }

    //
    // Sum the temporary histogram (Histogram2) with the caller-provided
    // histogram (Histogram1).
    //

    for (Index = 0; Index < 32; Index++) {

        //
        // Load a 32-byte chunk of each histogram into YMM registers.
        //

        Ymm0 = _mm256_load_si256(&Histogram1->Ymm[Index]);
        Ymm1 = _mm256_load_si256(&Histogram2->Ymm[Index]);

        //
        // Add the two vectors.
        //

        Ymm2 = _mm256_add_epi32(Ymm0, Ymm1);

        //
        // Store the result back in the first histogram.
        //

        _mm256_store_si256(&Histogram1->Ymm[Index], Ymm2);
    }

    //
    // Indicate success and return.
    //

    Success = TRUE;

    return Success;
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
