/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    HistogramInline.h

Abstract:

    This is the header file for inline function definitions related to
    histogram functionality.

--*/

FORCEINLINE
RTL_GENERIC_COMPARE_RESULTS
NTAPI
CompareHistogramsAlignedAvx2Inline(
    _In_ _Const_ PCCHARACTER_HISTOGRAM Left,
    _In_ _Const_ PCCHARACTER_HISTOGRAM Right
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

FORCEINLINE
BOOLEAN
NTAPI
CreateHistogramInline(
    _In_ PCLONG_STRING String,
    _Inout_updates_bytes_(sizeof(*Histogram)) PCHARACTER_HISTOGRAM Histogram
)
/*++

Routine Description:

    Creates a histogram from a given input string in a simple byte-by-byte
    fashion.

    N.B. Caller is responsible for ensuring that the memory backing the
         Histogram parameter has already been cleared.

Arguments:

    String - Supplies a pointer to the STRING structure containing the string
        for which the histogram is to be calculated.

    Histogram - Supplies a pointer to a CHARACTER_HISTOGRAM structure that will
        receive the calculated histogram for the given input string.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BYTE Byte;
    ULONG Index;
    PBYTE Buffer;
    PULONG Counts;

    //
    // Validate parameters.
    //

    if (!ARGUMENT_PRESENT(String)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Histogram)) {
        return FALSE;
    }

    //
    // Initialize aliases.
    //

    Buffer = String->Buffer;
    Counts = (PULONG)&Histogram->Counts;

    //
    // Loop over each byte in the input string and increment the corresponding
    // character count in the histogram.
    //

    for (Index = 0; Index < String->Length; Index++) {
        Byte = Buffer[Index];
        Counts[Byte]++;
    }

    //
    // Return success.
    //

    return TRUE;
}


FORCEINLINE
BOOLEAN
NTAPI
CreateHistogramAvx2Inline(
    _In_ PCLONG_STRING String,
    _Inout_updates_bytes_(sizeof(*Histogram))
        PCHARACTER_HISTOGRAM Histogram,
    _Inout_updates_bytes_(sizeof(*TempHistogram))
        PCHARACTER_HISTOGRAM TempHistogram
    )

/*++

Routine Description:

    Creates a histogram from a given input string using AVX2 intrinsics.

    N.B. Caller is responsible for ensuring that the memory backing the
         Histogram and TempHistogram parameters has already been cleared.

Arguments:

    String - Supplies a pointer to the STRING structure containing the string
        for which the histogram is to be calculated.

    Histogram - Supplies a pointer to a CHARACTER_HISTOGRAM structure that will
        receive the calculated histogram for the given input string.

    TempHistogram - Supplies a pointer to a second CHARACTER_HISTOGRAM structure
        receive the calculated histogram for the given input string.

Return Value:

    TRUE on success, FALSE on failure.

--*/
{
    BYTE Byte;
    ULONG Index;
    PBYTE Buffer;
    PULONG Counts;
    BOOLEAN Success;
    ULONGLONG Count;
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
    // Validate parameters.
    //

    if (!ARGUMENT_PRESENT(String)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(Histogram)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(TempHistogram)) {
        return FALSE;
    }

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

    //
    // If the buffer isn't aligned at a 32 byte boundary (or better), or the
    // number of bytes remaining in the string are less than or equal to 32,
    // just calculate the histogram using the old-fashioned, non-AVX approach.
    //

    while (Alignment < 32 || Remaining < 64) {

        Count = min(Alignment, (ULONGLONG)Remaining);

        //
        // Manually calculate the histogram for the first chunk of the string
        // buffer that isn't aligned on a 32-byte boundary.
        //

        for (Index = 0; Index < Count; Index++) {
            Byte = Buffer[Index];
            Counts[Byte]++;
        }

        //
        // Update the buffer and remaining count accordingly.
        //

        Buffer += Count;
        Remaining -= (LONGLONG)Count;

        //
        // Sanity check the number of bytes remaining is greater than or equal
        // to zero.  (If it has gone negative, we've got an issue with our Count
        // vs Remaining logic.)
        //

        ASSERT(Remaining >= 0);

        //
        // If there are no bytes remaining, fast-path jump to the end.
        //

        if (Remaining == 0) {
            goto End;
        }

        //
        // Obtain the new buffer alignment.
        //

        Alignment = GetAddressAlignment(Buffer);
    }

    //
    // Initialize the YMM buffer alias.
    //

    BufferYmm = (PYMMWORD)Buffer;

    //
    // Attempt to process as many 64 byte chunks as possible.
    //

    while (Alignment >= 32 && Remaining >= 64) {

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

        Alignment = GetAddressAlignment(BufferYmm);
        ASSERT(Alignment >= 32);

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

End:

    //
    // Indicate success and return.
    //

    Success = TRUE;

    return Success;
}

FORCEINLINE
BOOLEAN
NTAPI
CreateHistogramAvx2AlignedCInline(
    _In_ PCLONG_STRING String,
    _Inout_updates_bytes_(sizeof(*Histogram))
        PCHARACTER_HISTOGRAM Histogram,
    _Inout_updates_bytes_(sizeof(*TempHistogram))
        PCHARACTER_HISTOGRAM TempHistogram
    )

/*++

Routine Description:

    Creates a histogram from a given input string using AVX2 intrinsics.

    N.B. Caller is responsible for ensuring that the memory backing the
         Histogram and TempHistogram parameters has already been cleared.

Arguments:

    String - Supplies a pointer to the STRING structure containing the string
        for which the histogram is to be calculated.

    Histogram - Supplies a pointer to a CHARACTER_HISTOGRAM structure that will
        receive the calculated histogram for the given input string.

    TempHistogram - Supplies a pointer to a second CHARACTER_HISTOGRAM structure
        receive the calculated histogram for the given input string.

Return Value:

    TRUE on success, FALSE on failure.

--*/
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

FORCEINLINE
BOOLEAN
NTAPI
CreateHistogramAvx2AlignedC32Inline(
    _In_ PCLONG_STRING String,
    _Inout_updates_bytes_(sizeof(*Histogram))
        PCHARACTER_HISTOGRAM Histogram,
    _Inout_updates_bytes_(sizeof(*TempHistogram))
        PCHARACTER_HISTOGRAM TempHistogram
    )

/*++

Routine Description:

    Creates a histogram from a given input string using AVX2 intrinsics.

    N.B. Caller is responsible for ensuring that the memory backing the
         Histogram and TempHistogram parameters has already been cleared.

Arguments:

    String - Supplies a pointer to the STRING structure containing the string
        for which the histogram is to be calculated.

    Histogram - Supplies a pointer to a CHARACTER_HISTOGRAM structure that will
        receive the calculated histogram for the given input string.

    TempHistogram - Supplies a pointer to a second CHARACTER_HISTOGRAM structure
        receive the calculated histogram for the given input string.

Return Value:

    TRUE on success, FALSE on failure.

--*/
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
