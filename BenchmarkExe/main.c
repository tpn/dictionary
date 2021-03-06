/*++

Copyright (c) 2018 Trent Nelson <trent@trent.me>

Module Name:

    main.c

Abstract:

    Scratch/testing file.

--*/

#include "stdafx.h"

typedef __m512i DECLSPEC_ALIGN(64) ZMMWORD, *PZMMWORD, **PPZMMWORD;

RTL GlobalRtl;
ALLOCATOR GlobalAllocator;

PRTL Rtl;
PALLOCATOR Allocator;

DICTIONARY_FUNCTIONS GlobalApi;
PDICTIONARY_FUNCTIONS Api;

HMODULE GlobalModule = 0;

#define TIMESTAMP_TO_MICROSECONDS 1000000ULL
#define TIMESTAMP_TO_NANOSECONDS  1000000000ULL

DECLSPEC_ALIGN(64)
static const PCBYTE QuickLazy = \
    "The quick brown fox jumps over the lazy dog and then "
    "the lazy dog jumps over the quick brown fox.";

DECLSPEC_ALIGN(64)
static const PCBYTE AbcdRepeat = \
    "ABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCD"
    "ABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCD";

DECLSPEC_ALIGN(64)
static const PCBYTE AlphabetRepeat = \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!$";

DECLSPEC_ALIGN(64)
static const PCBYTE AlphabetRepeatx2 = \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!!"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!!";

#ifndef ASSERT
#define ASSERT(Condition) \
    if (!(Condition)) {   \
        __debugbreak();   \
    }
#endif

#define COMPARE(Comparison, Histogram1, Histogram2, Output)      \
    Comparison = Api->CompareHistograms(Histogram1, Histogram2); \
    if (Comparison != GenericEqual) {                            \
        SlowCompareHistogram(Histogram1, Histogram2, &Output);   \
        OUTPUT_FLUSH();                                          \
        __debugbreak();                                          \
    }

extern
BOOLEAN
Histo1544(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM_V4 Histogram
    );

extern
BOOLEAN
Histo1544v2(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM_V4 Histogram
    );


extern
BOOLEAN
Histo1710(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM_V4 Histogram
    );

extern
BOOLEAN
Histo1710v2(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM_V4 Histogram
    );

extern
BOOLEAN
Histo1710v3(
    PCLONG_STRING String,
    PCHARACTER_HISTOGRAM_V4 Histogram
    );


VOID
Scratch2(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    DICTIONARY_CREATE_FLAGS CreateFlags;
    PDICTIONARY Dictionary;
    BOOLEAN IsProcessTerminating;
    LONGLONG EntryCount;
    PDICTIONARY_STATS Stats;

    CreateFlags.AsULong = 0;
    IsProcessTerminating = FALSE;

    ASSERT(Api->CreateDictionary(Rtl,
                                 Allocator,
                                 CreateFlags,
                                 &Dictionary));

    ASSERT(Api->AddWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->GetDictionaryStats(Dictionary,
                                      Allocator,
                                      &Stats));

    ASSERT(Api->AddWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->RemoveWord(Dictionary,
                           "elbow",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "below",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "below",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "The quick brown fox jumped over the lazy dog.",
                           &EntryCount));

    ASSERT(Api->AddWord(Dictionary,
                           "The quick brown fox jumped over the lazy dog.",
                           &EntryCount));

    ASSERT(Api->DestroyDictionary(&Dictionary, &IsProcessTerminating));

    return;
}

BOOLEAN
MakeRandomString(
    PRTL Rtl,
    PALLOCATOR Allocator,
    ULONG BufferSize,
    PBYTE *BufferPointer
    )
{
    BOOLEAN Success;
    PBYTE Buffer;
    ULONGLONG BytesReplaced;

    *BufferPointer = NULL;

    Buffer = (PBYTE)Allocator->Malloc(Allocator, BufferSize);
    if (!Buffer) {
        return FALSE;
    }

    if (!Rtl->CryptGenRandom(Rtl, BufferSize, Buffer)) {
        goto Error;
    }

    Buffer[2] = 0x0;
    Buffer[57] = 0x0;

    BytesReplaced = Rtl->FindAndReplaceByte(BufferSize,
                                            Buffer,
                                            0x0,
                                            0xcc);

    if (BytesReplaced < 2) {
        __debugbreak();
        goto Error;
    }

    Success = TRUE;
    goto End;

Error:

    Success = FALSE;
    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    //
    // Intentional follow-on to End.
    //

End:

    *BufferPointer = Buffer;

    return Success;
}


typedef
_Success_(return != 0)
BOOLEAN
(NTAPI CREATE_BUFFER)(
    _In_ PRTL Rtl,
    _In_opt_ PHANDLE TargetProcessHandle,
    _In_ USHORT NumberOfPages,
    _In_opt_ PULONG AdditionalProtectionFlags,
    _Out_ PULONGLONG UsableBufferSizeInBytes,
    _Out_ PPVOID BufferAddress
    );
typedef CREATE_BUFFER *PCREATE_BUFFER;

CREATE_BUFFER CreateBuffer;

_Use_decl_annotations_
BOOLEAN
CreateBuffer(
    PRTL Rtl,
    PHANDLE TargetProcessHandle,
    USHORT NumberOfPages,
    PULONG AdditionalProtectionFlags,
    PULONGLONG UsableBufferSizeInBytes,
    PPVOID BufferAddress
    )
{
    BOOL Success;
    PVOID Buffer;
    PBYTE Unusable;
    HANDLE ProcessHandle;
    ULONG ProtectionFlags;
    ULONG OldProtectionFlags;
    LONG_INTEGER TotalNumberOfPages;
    ULARGE_INTEGER AllocSizeInBytes;
    ULARGE_INTEGER UsableSizeInBytes;

    //
    // Validate arguments.
    //

    if (!ARGUMENT_PRESENT(Rtl)) {
        return FALSE;
    }

    if (!ARGUMENT_PRESENT(TargetProcessHandle)) {
        ProcessHandle = GetCurrentProcess();
    } else {
        ProcessHandle = *TargetProcessHandle;
    }

    if (!ARGUMENT_PRESENT(UsableBufferSizeInBytes)) {
        return FALSE;
    } else {
        *UsableBufferSizeInBytes = 0;
    }

    if (!ARGUMENT_PRESENT(BufferAddress)) {
        return FALSE;
    } else {
        *BufferAddress = NULL;
    }

    TotalNumberOfPages.LongPart = (ULONG)NumberOfPages + 1;

    //
    // Verify the number of pages hasn't overflowed (i.e. exceeds max USHORT).
    //

    if (TotalNumberOfPages.HighPart) {
        return FALSE;
    }

    //
    // Convert total number of pages into total number of bytes (alloc size)
    // and verify it hasn't overflowed either (thus, 4GB is the current maximum
    // size allowed by this routine).
    //

    AllocSizeInBytes.QuadPart = TotalNumberOfPages.LongPart;
    AllocSizeInBytes.QuadPart <<= PAGE_SHIFT;

    if (AllocSizeInBytes.HighPart) {
        return FALSE;
    }

    ProtectionFlags = PAGE_READWRITE;
    if (ARGUMENT_PRESENT(AdditionalProtectionFlags)) {
        ProtectionFlags |= *AdditionalProtectionFlags;
    }

    //
    // Validation of parameters complete.  Proceed with buffer allocation.
    //

    Buffer = Rtl->VirtualAllocEx(ProcessHandle,
                                 NULL,
                                 AllocSizeInBytes.QuadPart,
                                 MEM_COMMIT,
                                 ProtectionFlags);

    if (!Buffer) {
        return FALSE;
    }

    //
    // Buffer was successfully allocated.  Any failures after this point should
    // `goto Error` to ensure the memory is freed.
    //
    // Calculate the usable size and corresponding unusable address.
    //

    UsableSizeInBytes.QuadPart = (
        (ULONGLONG)NumberOfPages <<
        (ULONGLONG)PAGE_SHIFT
    );

    Unusable = (PBYTE)Buffer;
    Unusable += UsableSizeInBytes.QuadPart;

    //
    // Change the protection of the trailing page to PAGE_NOACCESS.
    //

    ProtectionFlags = PAGE_NOACCESS;
    Success = Rtl->VirtualProtectEx(ProcessHandle,
                                    Unusable,
                                    PAGE_SIZE,
                                    ProtectionFlags,
                                    &OldProtectionFlags);

    if (!Success) {
        goto Error;
    }

    //
    // We're done, goto End.
    //

    Success = TRUE;
    goto End;

Error:

    Success = FALSE;

    //
    // Buffer should be non-NULL at this point.  Assert this invariant, free the
    // allocated memory, clear the buffer pointer and set the alloc size to 0.
    //

    ASSERT(Buffer);
    Rtl->VirtualFreeEx(ProcessHandle, Buffer, 0, MEM_RELEASE);
    Buffer = NULL;

    //
    // Intentional follow-on to End.
    //

End:

    //
    // Update caller's parameters and return.
    //

    *BufferAddress = Buffer;
    *UsableBufferSizeInBytes = UsableSizeInBytes.QuadPart;

    return Success;

}

typedef
BOOLEAN
(NTAPI TEST_CREATE_BUFFER)(
    _In_ PRTL Rtl,
    _In_ PCREATE_BUFFER CreateBuffer
    );
typedef TEST_CREATE_BUFFER *PTEST_CREATE_BUFFER;

TEST_CREATE_BUFFER TestCreateBuffer;

_Use_decl_annotations_
BOOLEAN
TestCreateBuffer(
    PRTL Rtl,
    PCREATE_BUFFER CreateBuffer
    )
{
    PVOID Address;
    PBYTE Unusable;
    BOOLEAN Success;
    BOOLEAN CaughtException;
    ULONGLONG Size;
    USHORT NumberOfPages;
    ULONG AdditionalProtectionFlags;

    NumberOfPages = 1;
    AdditionalProtectionFlags = 0;

    Success = CreateBuffer(Rtl,
                           NULL,
                           NumberOfPages,
                           &AdditionalProtectionFlags,
                           &Size,
                           &Address);

    if (!Success) {
        return FALSE;
    }

    Unusable = (PBYTE)RtlOffsetToPointer(Address, Size);

    CaughtException = FALSE;

    TRY_PROBE_MEMORY {

        *Unusable = 1;

    } CATCH_EXCEPTION_ACCESS_VIOLATION {

        CaughtException = TRUE;
    }

    Rtl->VirtualFreeEx(GetCurrentProcess(), Address, 0, MEM_RELEASE);

    if (!CaughtException) {
        return FALSE;
    }

    return TRUE;
}

VOID
AppendIntegerToCharBuffer(
    _Inout_ PPCHAR BufferPointer,
    _In_ ULONGLONG Integer
    )
{
    PCHAR Buffer;
    USHORT Offset;
    USHORT NumberOfDigits;
    ULONGLONG Digit;
    ULONGLONG Value;
    ULONGLONG Count;
    ULONGLONG Bytes;
    CHAR Char;
    PCHAR Dest;

    Buffer = *BufferPointer;

    //
    // Count the number of digits required to represent the integer in base 10.
    //

    NumberOfDigits = CountNumberOfLongLongDigitsInline(Integer);

    //
    // Initialize our destination pointer to the last digit.  (We write
    // back-to-front.)
    //

    Offset = (NumberOfDigits - 1) * sizeof(Char);
    Dest = (PCHAR)RtlOffsetToPointer(Buffer, Offset);

    Count = 0;
    Bytes = 0;

    //
    // Convert each digit into the corresponding character and copy to the
    // string buffer, retreating the pointer as we go.
    //

    Value = Integer;

    do {
        Count++;
        Bytes += sizeof(Char);
        Digit = Value % 10;
        Value = Value / 10;
        Char = ((CHAR)Digit + '0');
        *Dest-- = Char;
    } while (Value != 0);

    *BufferPointer = RtlOffsetToPointer(Buffer, Bytes);

    return;
}

VOID
AppendStringToCharBuffer(
    _Inout_ PPCHAR BufferPointer,
    _In_ PSTRING String
    )
{
    PVOID Buffer;

    Buffer = *BufferPointer;
    CopyMemory(Buffer, String->Buffer, String->Length);
    *BufferPointer = RtlOffsetToPointer(Buffer, String->Length);

    return;
}

VOID
AppendCharBufferToCharBuffer(
    _Inout_ PPCHAR BufferPointer,
    _In_ PCHAR String,
    _In_ ULONG SizeInBytes
    )
{
    PVOID Buffer;

    Buffer = *BufferPointer;
    CopyMemory(Buffer, String, SizeInBytes);
    *BufferPointer = RtlOffsetToPointer(Buffer, SizeInBytes);

    return;
}

FORCEINLINE
VOID
AppendCharToCharBuffer(
    _Inout_ PPCHAR BufferPointer,
    _In_ CHAR Char
    )
{
    PCHAR Buffer;

    Buffer = *BufferPointer;
    *Buffer = Char;
    *BufferPointer = Buffer + 1;
}

#define OUTPUT_RAW(String)                                          \
    AppendCharBufferToCharBuffer(&Output, String, sizeof(String)-1)

#define OUTPUT_STRING(String) AppendStringToCharBuffer(&Output, String)

#define OUTPUT_CHR(Char) AppendCharToCharBuffer(&Output, Char)
#define OUTPUT_SEP() AppendCharToCharBuffer(&Output, ',')
#define OUTPUT_LF() AppendCharToCharBuffer(&Output, '\n')

#define OUTPUT_INT(Value)                      \
    AppendIntegerToCharBuffer(&Output, Value);

#define OUTPUT_FLUSH_CONSOLE()                                               \
    BytesToWrite.QuadPart = ((ULONG_PTR)Output) - ((ULONG_PTR)OutputBuffer); \
    Success = WriteConsoleA(OutputHandle,                                    \
                            OutputBuffer,                                    \
                            BytesToWrite.LowPart,                            \
                            &CharsWritten,                                   \
                            NULL);                                           \
    ASSERT(Success);                                                         \
    Output = OutputBuffer

#define OUTPUT_FLUSH_FILE()                                                    \
    BytesToWrite.QuadPart = ((ULONG_PTR)Output) - ((ULONG_PTR)OutputBuffer)-1; \
    Success = WriteFile(OutputHandle,                                          \
                        OutputBuffer,                                          \
                        BytesToWrite.LowPart,                                  \
                        &BytesWritten,                                         \
                        NULL);                                                 \
    ASSERT(Success);                                                           \
    Output = OutputBuffer

#define OUTPUT_FLUSH()                                                       \
    BytesToWrite.QuadPart = ((ULONG_PTR)Output) - ((ULONG_PTR)OutputBuffer); \
    Success = WriteConsoleA(OutputHandle,                                    \
                            OutputBuffer,                                    \
                            BytesToWrite.LowPart,                            \
                            &CharsWritten,                                   \
                            NULL);                                           \
    if (!Success) {                                                          \
        Success = WriteFile(OutputHandle,                                    \
                            OutputBuffer,                                    \
                            BytesToWrite.LowPart,                            \
                            &BytesWritten,                                   \
                            NULL);                                           \
        ASSERT(Success);                                                     \
    }                                                                        \
    Output = OutputBuffer


#pragma optimize("", off)
NOINLINE
VOID
FillBufferWithBytes(
    PBYTE Dest,
    PCBYTE Source,
    ULONGLONG SizeOfDest,
    ULONGLONG SizeOfSource
    )
{
    ULONGLONG Index;
    ULONGLONG Count;
    ULONGLONG Total;
    YMMWORD Chunk1;
    YMMWORD Chunk2;
    PYMMWORD DestYmm;
    PYMMWORD SourceYmm;

    DestYmm = (PYMMWORD)Dest;
    SourceYmm = (PYMMWORD)Source;

    Total = SizeOfDest;
    Count = Total >> 6;

    Chunk1 = _mm256_loadu_si256((PYMMWORD)SourceYmm);
    Chunk2 = _mm256_loadu_si256((PYMMWORD)SourceYmm+1);

    for (Index = 0; Index < Count; Index++) {
        _mm256_store_si256((PYMMWORD)DestYmm, Chunk1);
        _mm256_store_si256((PYMMWORD)DestYmm+1, Chunk2);
        DestYmm += 2;
    }
}
#pragma optimize("", on)

VOID
Scratch3(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api,
    ULONG BufferSize,
    PULONGLONG Nanoseconds1Pointer,
    PULONGLONG Nanoseconds2Pointer
    )
{
    BOOL Success;
    ULONG OldCodePage;
    PBYTE Buffer;
    ULARGE_INTEGER BytesToWrite;
    LONG_STRING String;
    BOOLEAN Result1;
    BOOLEAN Result2;
    CHARACTER_HISTOGRAM HistogramA;
    CHARACTER_HISTOGRAM_V4 HistogramB;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;
    RTL_GENERIC_COMPARE_RESULTS Comparison;
    HANDLE OutputHandle;
    LARGE_INTEGER Frequency;
    LARGE_INTEGER Start1;
    LARGE_INTEGER Start2;
    LARGE_INTEGER End1;
    LARGE_INTEGER End2;
    LARGE_INTEGER Elapsed1;
    LARGE_INTEGER Elapsed2;
    LARGE_INTEGER Microseconds1;
    LARGE_INTEGER Microseconds2;
    LARGE_INTEGER Nanoseconds1;
    LARGE_INTEGER Nanoseconds2;
    LARGE_INTEGER Multiplicand = { TIMESTAMP_TO_NANOSECONDS };
    ULONGLONG OutputBufferSize;
    ULONG BytesWritten;
    ULONG CharsWritten;
    PCHAR Output;
    PCHAR OutputBuffer;
    UNICODE_STRING TempW = RTL_CONSTANT_STRING(L"Test unicode.\r\n");
    STRING TempA = RTL_CONSTANT_STRING("Test ascii.\r\n");

    ZeroStruct(HistogramA);
    ZeroStruct(HistogramB);

    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    ASSERT(OutputHandle);

    Success = WriteConsoleW(OutputHandle,
                            TempW.Buffer,
                            TempW.Length >> 1,
                            &CharsWritten, NULL);

    OldCodePage = GetConsoleCP();
    ASSERT(SetConsoleCP(20127));

    Success = WriteConsoleA(OutputHandle,
                            TempA.Buffer, TempA.Length, &CharsWritten, NULL);

    ASSERT(
        MakeRandomString(Rtl,
                         Allocator,
                         BufferSize,
                         &Buffer)
    );

    Success = CreateBuffer(Rtl, NULL, 1, 0, &OutputBufferSize, &OutputBuffer);
    ASSERT(Success);

    Output = OutputBuffer;


    String.Length = BufferSize;
    String.Hash = 0;
    String.Buffer = Buffer;

    QueryPerformanceFrequency(&Frequency);

    QueryPerformanceCounter(&Start1);
    Result1 = Api->CreateHistogram(&String, &HistogramA);
    QueryPerformanceCounter(&End1);

    Elapsed1.QuadPart = End1.QuadPart - Start1.QuadPart;

    ASSERT(Result1);

    QueryPerformanceCounter(&Start2);
    Result2 = Api->CreateHistogramAvx2C(&String,
                                        &HistogramB.Histogram1,
                                        &HistogramB.Histogram2);
    QueryPerformanceCounter(&End2);

    Elapsed2.QuadPart = End2.QuadPart - Start2.QuadPart;

    ASSERT(Result2);

    Nanoseconds1.QuadPart = Elapsed1.QuadPart * TIMESTAMP_TO_NANOSECONDS;
    Nanoseconds1.QuadPart /= Frequency.QuadPart;

    Nanoseconds2.QuadPart = Elapsed2.QuadPart * TIMESTAMP_TO_NANOSECONDS;
    Nanoseconds2.QuadPart /= Frequency.QuadPart;

    Microseconds1.QuadPart = Elapsed1.QuadPart * TIMESTAMP_TO_MICROSECONDS;
    Microseconds1.QuadPart /= Frequency.QuadPart;

    Microseconds2.QuadPart = Elapsed2.QuadPart * TIMESTAMP_TO_MICROSECONDS;
    Microseconds2.QuadPart /= Frequency.QuadPart;

    Histogram1 = &HistogramA;
    Histogram2 = &HistogramB.Histogram1;

    Comparison = Api->CompareHistograms(Histogram1, Histogram2);

    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    //ASSERT(Elapsed2.QuadPart < Elapsed1.QuadPart);
    ASSERT(Comparison == GenericEqual);

    OUTPUT_RAW("CreateHistogram: ");
    OUTPUT_INT(Nanoseconds1.QuadPart);
    OUTPUT_RAW(" ns (");
    OUTPUT_INT(Microseconds1.QuadPart);
    OUTPUT_RAW(" us)\r\n");
    OUTPUT_FLUSH();

    OUTPUT_RAW("CreateHistogramAvx2: ");
    OUTPUT_INT(Nanoseconds2.QuadPart);
    OUTPUT_RAW(" ns (");
    OUTPUT_INT(Microseconds2.QuadPart);
    OUTPUT_RAW(" us)\r\n");
    OUTPUT_FLUSH();

    ASSERT(SetConsoleCP(OldCodePage));

}

typedef struct _TIMESTAMP {
    ULONGLONG Id;
    ULONGLONG Count;
    STRING Name;
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    ULARGE_INTEGER StartTsc;
    ULARGE_INTEGER EndTsc;
    ULARGE_INTEGER Tsc;
    ULARGE_INTEGER TotalTsc;
    ULARGE_INTEGER MinimumTsc;
    ULARGE_INTEGER MaximumTsc;
    ULARGE_INTEGER Cycles;
    ULARGE_INTEGER MinimumCycles;
    ULARGE_INTEGER MaximumCycles;
    ULARGE_INTEGER TotalCycles;
    ULARGE_INTEGER Nanoseconds;
    ULARGE_INTEGER TotalNanoseconds;
    ULARGE_INTEGER MinimumNanoseconds;
    ULARGE_INTEGER MaximumNanoseconds;
} TIMESTAMP;
typedef TIMESTAMP *PTIMESTAMP;

#define INIT_TIMESTAMP(Idx, Namex)                               \
    ZeroStruct(Timestamp##Idx);                                  \
    Timestamp##Idx##.Id = Idx;                                   \
    Timestamp##Idx##.Name.Length = sizeof(Namex)-1;              \
    Timestamp##Idx##.Name.MaximumLength = sizeof(Namex);         \
    Timestamp##Idx##.Name.Buffer = Namex;                        \
    Timestamp##Idx##.TotalTsc.QuadPart = 0;                      \
    Timestamp##Idx##.TotalCycles.QuadPart = 0;                   \
    Timestamp##Idx##.TotalNanoseconds.QuadPart = 0;              \
    Timestamp##Idx##.MinimumTsc.QuadPart = (ULONGLONG)-1;        \
    Timestamp##Idx##.MinimumCycles.QuadPart = (ULONGLONG)-1;     \
    Timestamp##Idx##.MinimumNanoseconds.QuadPart = (ULONGLONG)-1

#define RESET_TIMESTAMP(Id)                                      \
    Timestamp##Id##.Count = 0;                                   \
    Timestamp##Id##.TotalTsc.QuadPart = 0;                       \
    Timestamp##Id##.TotalCycles.QuadPart = 0;                    \
    Timestamp##Id##.TotalNanoseconds.QuadPart = 0;               \
    Timestamp##Id##.MinimumTsc.QuadPart = (ULONGLONG)-1;         \
    Timestamp##Id##.MaximumTsc.QuadPart = 0;                     \
    Timestamp##Id##.MinimumCycles.QuadPart = (ULONGLONG)-1;      \
    Timestamp##Id##.MaximumCycles.QuadPart = 0;                  \
    Timestamp##Id##.MinimumNanoseconds.QuadPart = (ULONGLONG)-1; \
    Timestamp##Id##.MaximumNanoseconds.QuadPart = 0

#define START_TIMESTAMP(Id)                          \
    ++Timestamp##Id##.Count;                         \
    QueryPerformanceCounter(&Timestamp##Id##.Start); \
    Timestamp##Id##.StartTsc.QuadPart = __rdtsc()

#define END_TIMESTAMP(Id)                                       \
    Timestamp##Id##.EndTsc.QuadPart = __rdtsc();                \
    QueryPerformanceCounter(&Timestamp##Id##.End);              \
    Timestamp##Id##.Tsc.QuadPart = (                            \
        Timestamp##Id##.EndTsc.QuadPart -                       \
        Timestamp##Id##.StartTsc.QuadPart                       \
    );                                                          \
    Timestamp##Id##.Cycles.QuadPart = (                         \
        Timestamp##Id##.End.QuadPart -                          \
        Timestamp##Id##.Start.QuadPart                          \
    );                                                          \
    Timestamp##Id##.TotalTsc.QuadPart += (                      \
        Timestamp##Id##.Tsc.QuadPart                            \
    );                                                          \
    Timestamp##Id##.TotalCycles.QuadPart += (                   \
        Timestamp##Id##.Cycles.QuadPart                         \
    );                                                          \
    Timestamp##Id##.Nanoseconds.QuadPart = (                    \
        Timestamp##Id##.Cycles.QuadPart *                       \
        TIMESTAMP_TO_NANOSECONDS                                \
    );                                                          \
    Timestamp##Id##.Nanoseconds.QuadPart /= Frequency.QuadPart; \
    Timestamp##Id##.TotalNanoseconds.QuadPart += (              \
        Timestamp##Id##.Nanoseconds.QuadPart                    \
    );                                                          \
    if (Timestamp##Id##.MinimumNanoseconds.QuadPart >           \
        Timestamp##Id##.Nanoseconds.QuadPart) {                 \
            Timestamp##Id##.MinimumNanoseconds.QuadPart = (     \
                Timestamp##Id##.Nanoseconds.QuadPart            \
            );                                                  \
    }                                                           \
    if (Timestamp##Id##.MaximumNanoseconds.QuadPart <           \
        Timestamp##Id##.Nanoseconds.QuadPart) {                 \
            Timestamp##Id##.MaximumNanoseconds.QuadPart = (     \
                Timestamp##Id##.Nanoseconds.QuadPart            \
            );                                                  \
    }                                                           \
    if (Timestamp##Id##.MinimumTsc.QuadPart >                   \
        Timestamp##Id##.Tsc.QuadPart) {                         \
            Timestamp##Id##.MinimumTsc.QuadPart = (             \
                Timestamp##Id##.Tsc.QuadPart                    \
            );                                                  \
    }                                                           \
    if (Timestamp##Id##.MaximumTsc.QuadPart <                   \
        Timestamp##Id##.Tsc.QuadPart) {                         \
            Timestamp##Id##.MaximumTsc.QuadPart = (             \
                Timestamp##Id##.Tsc.QuadPart                    \
            );                                                  \
    }                                                           \
    if (Timestamp##Id##.MinimumCycles.QuadPart >                \
        Timestamp##Id##.Cycles.QuadPart) {                      \
            Timestamp##Id##.MinimumCycles.QuadPart = (          \
                Timestamp##Id##.Cycles.QuadPart                 \
            );                                                  \
    }                                                           \
    if (Timestamp##Id##.MaximumCycles.QuadPart <                \
        Timestamp##Id##.Cycles.QuadPart) {                      \
            Timestamp##Id##.MaximumCycles.QuadPart = (          \
                Timestamp##Id##.Cycles.QuadPart                 \
            );                                                  \
    }

#define FINISH_TIMESTAMP(Id, Length, Iterations)             \
    OUTPUT_STRING(&Timestamp##Id##.Name);                    \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(*Length);                                     \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Iterations);                                  \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.MinimumTsc.QuadPart);         \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.MaximumTsc.QuadPart);         \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.TotalTsc.QuadPart);           \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.MinimumCycles.QuadPart);      \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.MaximumCycles.QuadPart);      \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.TotalCycles.QuadPart);        \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.MinimumNanoseconds.QuadPart); \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.MaximumNanoseconds.QuadPart); \
    OUTPUT_SEP();                                            \
    OUTPUT_INT(Timestamp##Id##.TotalNanoseconds.QuadPart);   \
    OUTPUT_LF()

VOID
Scratch4(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    BOOL Success;
    ULONG Index;
    ULONG Iterations;
    ULONG OldCodePage;
    PBYTE Buffer;
    ULARGE_INTEGER BytesToWrite;
    LONG_STRING String;
    BOOLEAN Result;
    CHARACTER_HISTOGRAM HistogramA;
    CHARACTER_HISTOGRAM_V4 HistogramB;
    HANDLE OutputHandle;
    LARGE_INTEGER Frequency;
    TIMESTAMP Timestamp1;
    TIMESTAMP Timestamp2;
    ULONG BufferSize = 1 << 16;
    ULONGLONG OutputBufferSize;
    ULONG BytesWritten;
    ULONG CharsWritten;
    PCHAR Output;
    PCHAR OutputBuffer;
    ULONG Lengths[] = {
        1,
        5,
        7,
        10,
        15,
        18,
        31,
        39,
        50,
        60,
        64,
        100,
        200,
        3000,
        0
    };
    PULONG Length;

    ZeroStruct(HistogramA);
    ZeroStruct(HistogramB);

    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    ASSERT(OutputHandle);

    OldCodePage = GetConsoleCP();

    ASSERT(SetConsoleCP(20127));

    ASSERT(
        MakeRandomString(Rtl,
                         Allocator,
                         BufferSize,
                         &Buffer)
    );

    Success = CreateBuffer(Rtl, NULL, 1, 0, &OutputBufferSize, &OutputBuffer);
    ASSERT(Success);

    Output = OutputBuffer;

    String.Length = BufferSize;
    String.Hash = 0;
    String.Buffer = Buffer;

    QueryPerformanceFrequency(&Frequency);


    INIT_TIMESTAMP(1, "CreateHistogram     ");
    INIT_TIMESTAMP(2, "CreateHistogramAvx2C");

    OUTPUT_RAW("Name,Length,Iterations,Minimum,Maximum\n");

    Iterations = 1000;
    Length = Lengths;

    do {
        String.Length = *Length;

        RESET_TIMESTAMP(1);
        for (Index = 0; Index < Iterations; Index++) {
            START_TIMESTAMP(1);
            Result = Api->CreateHistogram(&String, &HistogramA);
            END_TIMESTAMP(1);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(1, Length, Iterations);

        OUTPUT_FLUSH();

        RESET_TIMESTAMP(2);
        for (Index = 0; Index < Iterations; Index++) {
            START_TIMESTAMP(2);
            Result = Api->CreateHistogramAvx2C(&String,
                                               &HistogramB.Histogram1,
                                               &HistogramB.Histogram2);
            END_TIMESTAMP(2);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(2, Length, Iterations);

        OUTPUT_FLUSH();
    } while (*(++Length));

    /*
    Length = Lengths;

    do {
        RESET_TIMESTAMP(2);
        for (Index = 0; Index < Iterations; Index++) {
            START_TIMESTAMP(2);
            Result = Api->CreateHistogramAvx2C(&String,
                                               &HistogramB.Histogram1,
                                               &HistogramB.Histogram2);
            END_TIMESTAMP(2);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(2, Length, Iterations);
        OUTPUT_FLUSH2();
    } while (*(++Length));
    */

    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    OUTPUT_FLUSH();

    ASSERT(SetConsoleCP(OldCodePage));

}

VOID
SlowCompareHistogram(
    _In_ _Const_ PCCHARACTER_HISTOGRAM Left,
    _In_ _Const_ PCCHARACTER_HISTOGRAM Right,
    _Inout_ PPCHAR OutputPointer
    )
{
    BYTE Index;
    ULONG LeftCount;
    ULONG RightCount;
    PCHAR Output;
    Output = *OutputPointer;

    for (Index = 0; Index < 10; Index++) {
        LeftCount = Left->Counts[Index];
        RightCount = Right->Counts[Index];
        if (LeftCount == RightCount) {
            continue;
        }
        OUTPUT_RAW("[  ");
        OUTPUT_INT(Index);
        OUTPUT_CHR(' ');
        OUTPUT_CHR((CHAR)Index);
        OUTPUT_RAW("]:\t");
        OUTPUT_INT(LeftCount);
        if (LeftCount != RightCount) {
            OUTPUT_RAW("\t!=\t");
        } else {
            OUTPUT_RAW("\t==\t");
        }
        OUTPUT_INT(RightCount);
        OUTPUT_RAW("\n");
    }

    for (Index = 10; Index < 100; Index++) {
        LeftCount = Left->Counts[Index];
        RightCount = Right->Counts[Index];
        if (LeftCount == RightCount) {
            continue;
        }
        OUTPUT_RAW("[ ");
        OUTPUT_INT(Index);
        OUTPUT_CHR(' ');
        OUTPUT_CHR((CHAR)Index);
        OUTPUT_RAW("]:\t");
        OUTPUT_INT(LeftCount);
        if (LeftCount != RightCount) {
            OUTPUT_RAW("\t!=\t");
        } else {
            OUTPUT_RAW("\t==\t");
        }
        OUTPUT_INT(RightCount);
        OUTPUT_RAW("\n");
    }

    for (Index = 100; Index < 255; Index++) {
        LeftCount = Left->Counts[Index];
        RightCount = Right->Counts[Index];
        if (LeftCount == RightCount) {
            continue;
        }
        OUTPUT_RAW("[");
        OUTPUT_INT(Index);
        OUTPUT_CHR(' ');
        OUTPUT_CHR((CHAR)Index);
        OUTPUT_RAW("]:\t");
        OUTPUT_INT(LeftCount);
        if (LeftCount != RightCount) {
            OUTPUT_RAW("\t!=\t");
        } else {
            OUTPUT_RAW("\t==\t");
        }
        OUTPUT_INT(RightCount);
        OUTPUT_RAW("\n");
    }

    *OutputPointer = Output;
}


#pragma optimize("", off)
NOINLINE
VOID
CanWeUseAvx512(PBOOLEAN UseAvx512Pointer)
{
    BOOLEAN UseAvx512 = TRUE;
    TRY_AVX512 {
        ZMMWORD Test1 = _mm512_set1_epi64(1);
        ZMMWORD Test2 = _mm512_add_epi64(Test1, Test1);
        UNREFERENCED_PARAMETER(Test2);
    } CATCH_EXCEPTION_ILLEGAL_INSTRUCTION{
        UseAvx512 = FALSE;
    }
    *UseAvx512Pointer = UseAvx512;
}
#pragma optimize("", on)

VOID
Scratch5(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    BOOL Success;
    ULONG Index;
    ULONG Iterations;
    ULONG OldCodePage;
    PBYTE Buffer;
    ULARGE_INTEGER BytesToWrite;
    LONG_STRING String;
    BOOLEAN Result;
    BOOLEAN UseAvx512;
    CHARACTER_HISTOGRAM HistogramA;
    CHARACTER_HISTOGRAM_V4 HistogramB;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;
    HANDLE OutputHandle;
    LARGE_INTEGER Frequency;
    TIMESTAMP Timestamp1;
    TIMESTAMP Timestamp2;
    TIMESTAMP Timestamp3;
    TIMESTAMP Timestamp4;
    TIMESTAMP Timestamp5;
    TIMESTAMP Timestamp6;
    TIMESTAMP Timestamp7;
    TIMESTAMP Timestamp8;
    TIMESTAMP Timestamp9;
    TIMESTAMP Timestamp10;
    TIMESTAMP Timestamp11;
    TIMESTAMP Timestamp12;
    TIMESTAMP Timestamp13;
    TIMESTAMP Timestamp14;
    TIMESTAMP Timestamp15;
    TIMESTAMP Timestamp16;
    TIMESTAMP Timestamp17;
    TIMESTAMP Timestamp18;
    TIMESTAMP Timestamp19;
    TIMESTAMP Timestamp20;
    RTL_GENERIC_COMPARE_RESULTS Comparison;
    ULONG BufferSize = 1 << 23;
    ULONGLONG OutputBufferSize;
    ULONG BytesWritten;
    ULONG CharsWritten;
    PCHAR Output;
    PCHAR OutputBuffer;
    ULONG Lengths[] = {
        64,
        128,
        192,
        256,
        384,
        512,
        1024,
        2048,
        4096,
        8192,
        16384,
        32768,
        65536,
        1 << 17,
        1 << 18,
        1 << 19,
        0
    };
    PULONG Length;

    ZeroStruct(HistogramA);
    ZeroStruct(HistogramB);

    Histogram1 = &HistogramA;
    Histogram2 = &HistogramB.Histogram1;

    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    ASSERT(OutputHandle);

    OldCodePage = GetConsoleCP();

    ASSERT(SetConsoleCP(20127));

    ASSERT(
        MakeRandomString(Rtl,
                         Allocator,
                         BufferSize,
                         &Buffer)
    );

    Success = CreateBuffer(Rtl, NULL, 1, 0, &OutputBufferSize, &OutputBuffer);
    ASSERT(Success);

    Output = OutputBuffer;

    String.Length = BufferSize;
    String.Hash = 0;
    String.Buffer = Buffer;

    QueryPerformanceFrequency(&Frequency);

    CanWeUseAvx512(&UseAvx512);

    INIT_TIMESTAMP(1,  "CreateHistogram                     ");
    INIT_TIMESTAMP(19, "CreateHistogramAlignedAsm           ");
    INIT_TIMESTAMP(20, "CreateHistogramAlignedAsm_v2        ");
    INIT_TIMESTAMP(2,  "CreateHistogramAvx2C                ");
    INIT_TIMESTAMP(3,  "CreateHistogramAvx2AlignedC         ");
    INIT_TIMESTAMP(4,  "CreateHistogramAvx2AlignedC32       ");
    INIT_TIMESTAMP(5,  "CreateHistogramAvx2AlignedCV4       ");
    INIT_TIMESTAMP(6,  "CreateHistogramAvx2AlignedAsm       ");
    INIT_TIMESTAMP(7,  "CreateHistogramAvx2AlignedAsm_v2    ");
    INIT_TIMESTAMP(8,  "CreateHistogramAvx2AlignedAsm_v3    ");
    INIT_TIMESTAMP(11, "CreateHistogramAvx2AlignedAsm_v4    ");
    INIT_TIMESTAMP(12, "CreateHistogramAvx2AlignedAsm_v5    ");
    INIT_TIMESTAMP(13, "CreateHistogramAvx2AlignedAsm_v5_2  ");
    INIT_TIMESTAMP(14, "CreateHistogramAvx2AlignedAsm_v5_3  ");
    INIT_TIMESTAMP(15, "CreateHistogramAvx2AlignedAsm_v5_3_2");
    INIT_TIMESTAMP(16, "CreateHistogramAvx2AlignedAsm_v5_3_3");

    INIT_TIMESTAMP(9,  "CreateHistogramAvx512AlignedAsm     ");
    INIT_TIMESTAMP(10, "CreateHistogramAvx512AlignedAsm_v2  ");
    INIT_TIMESTAMP(17, "CreateHistogramAvx512AlignedAsm_v3  ");
    INIT_TIMESTAMP(18, "CreateHistogramAvx512AlignedAsm_v4  ");

    OUTPUT_RAW("Name,Length,Iterations,"
               "MinimumTsc,MaximumTsc,TotalTsc,"
               "MinimumCycles,MaximumCycles,TotalCycles,"
               "MinimumNanoseconds,MaximumNanoseconds,TotalNanoseconds\n");

    Iterations = 5000;
    Length = Lengths;

    do {
        String.Length = *Length;

        ZeroStruct(HistogramA);
        Result = Api->CreateHistogram(&String, Histogram1);
        ASSERT(Result);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAlignedAsm(&String, &HistogramB));
        COMPARE(Comparison, Histogram1, Histogram2, Output);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAlignedAsm_v2(&String, &HistogramB));
        COMPARE(Comparison, Histogram1, Histogram2, Output);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v2(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v3(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v4(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v5(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v5_2(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v5_3(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);


        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx2AlignedAsm_v5_3_2(&String, &HistogramB));
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx2AlignedAsm_v5_3_3(&String, &HistogramB));
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        if (UseAvx512) {
            ZeroStruct(HistogramB);
            ASSERT(Api->CreateHistogramAvx512AlignedAsm_v4(&String, &HistogramB));
            COMPARE(Comparison, Histogram1, Histogram2, Output);
        }

        RESET_TIMESTAMP(1);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramA);
            START_TIMESTAMP(1);
            Result = Api->CreateHistogram(&String, &HistogramA);
            END_TIMESTAMP(1);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(1, Length, Iterations);

        OUTPUT_FLUSH();

        RESET_TIMESTAMP(19);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(19);
            Result = Api->CreateHistogramAlignedAsm(&String, &HistogramB);
            END_TIMESTAMP(19);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(19, Length, Iterations);


        RESET_TIMESTAMP(20);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(20);
            Result = Api->CreateHistogramAlignedAsm_v2(&String, &HistogramB);
            END_TIMESTAMP(20);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(20, Length, Iterations);


        RESET_TIMESTAMP(2);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(2);
            Result = Api->CreateHistogramAvx2C(&String,
                                               &HistogramB.Histogram1,
                                               &HistogramB.Histogram2);
            END_TIMESTAMP(2);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(2, Length, Iterations);

        RESET_TIMESTAMP(3);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(3);
            Result = Api->CreateHistogramAvx2AlignedC(&String,
                                                      &HistogramB.Histogram1,
                                                      &HistogramB.Histogram2);
            END_TIMESTAMP(3);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(3, Length, Iterations);

        RESET_TIMESTAMP(4);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(4);
            Result = Api->CreateHistogramAvx2AlignedC32(&String,
                                                        &HistogramB.Histogram1,
                                                        &HistogramB.Histogram2);
            END_TIMESTAMP(4);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(4, Length, Iterations);

        RESET_TIMESTAMP(5);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(5);
            Result = Api->CreateHistogramAvx2AlignedCV4(&String,
                                                        &HistogramB);
            END_TIMESTAMP(5);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(5, Length, Iterations);

        OUTPUT_FLUSH();

        RESET_TIMESTAMP(6);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(6);
            Result = Api->CreateHistogramAvx2AlignedAsm(&String,
                                                        &HistogramB);
            END_TIMESTAMP(6);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(6, Length, Iterations);

        RESET_TIMESTAMP(7);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(7);
            Result = Api->CreateHistogramAvx2AlignedAsm_v2(&String,
                                                           &HistogramB);
            END_TIMESTAMP(7);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(7, Length, Iterations);

        RESET_TIMESTAMP(8);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(8);
            Result = Api->CreateHistogramAvx2AlignedAsm_v3(&String,
                                                           &HistogramB);
            END_TIMESTAMP(8);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(8, Length, Iterations);

        RESET_TIMESTAMP(11);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(11);
            Result = Api->CreateHistogramAvx2AlignedAsm_v4(&String,
                                                           &HistogramB);
            END_TIMESTAMP(11);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(11, Length, Iterations);

        OUTPUT_FLUSH();

        RESET_TIMESTAMP(12);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(12);
            Result = Api->CreateHistogramAvx2AlignedAsm_v5(&String,
                                                           &HistogramB);
            END_TIMESTAMP(12);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(12, Length, Iterations);

        RESET_TIMESTAMP(13);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(13);
            Result = Api->CreateHistogramAvx2AlignedAsm_v5_2(&String,
                                                             &HistogramB);
            END_TIMESTAMP(13);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(13, Length, Iterations);

        RESET_TIMESTAMP(14);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(14);
            Result = Api->CreateHistogramAvx2AlignedAsm_v5_3(&String,
                                                             &HistogramB);
            END_TIMESTAMP(14);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(14, Length, Iterations);

        RESET_TIMESTAMP(15);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(15);
            Result = Api->CreateHistogramAvx2AlignedAsm_v5_3_2(&String,
                                                               &HistogramB);
            END_TIMESTAMP(15);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(15, Length, Iterations);

        OUTPUT_FLUSH();

        RESET_TIMESTAMP(16);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(16);
            Result = Api->CreateHistogramAvx2AlignedAsm_v5_3_3(&String,
                                                               &HistogramB);
            END_TIMESTAMP(16);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(16, Length, Iterations);

        if (!UseAvx512) {
            OUTPUT_FLUSH();
            continue;
        }

        RESET_TIMESTAMP(9);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(9);
            Result = Api->CreateHistogramAvx512AlignedAsm(&String,
                                                          &HistogramB);
            END_TIMESTAMP(9);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(9, Length, Iterations);

        RESET_TIMESTAMP(10);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(10);
            Result = Api->CreateHistogramAvx512AlignedAsm_v2(&String,
                                                             &HistogramB);
            END_TIMESTAMP(10);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(10, Length, Iterations);

        OUTPUT_FLUSH();

        RESET_TIMESTAMP(17);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(17);
            Result = Api->CreateHistogramAvx512AlignedAsm_v3(&String,
                                                             &HistogramB);
            END_TIMESTAMP(17);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(17, Length, Iterations);

        RESET_TIMESTAMP(18);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(18);
            Result = Api->CreateHistogramAvx512AlignedAsm_v4(&String,
                                                             &HistogramB);
            END_TIMESTAMP(18);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(18, Length, Iterations);

        OUTPUT_FLUSH();
    } while (*(++Length));

    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    OUTPUT_FLUSH();

    ASSERT(SetConsoleCP(OldCodePage));

}

VOID
Scratch7(
    VOID
    )
{
    ZMMWORD Index = _mm512_setzero_si512();
    ZMMWORD Values = _mm512_set1_epi32(10);
    ZMMWORD Result;

    Result = _mm512_permutexvar_epi16(Index, Values);

}

VOID
Scratch6(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    BOOL Success;
    ULONG Index;
    ULONG Iterations;
    ULONG OldCodePage;
    PBYTE Buffer;
    ULARGE_INTEGER BytesToWrite;
    LONG_STRING String;
    BOOLEAN Result;
    CHARACTER_HISTOGRAM_V4 HistogramA;
    CHARACTER_HISTOGRAM_V4 HistogramB;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;
    HANDLE OutputHandle;
    LARGE_INTEGER Frequency;
    TIMESTAMP Timestamp1;
    TIMESTAMP Timestamp2;
    ULONG BufferSize = 1 << 23;
    ULONGLONG OutputBufferSize;
    RTL_GENERIC_COMPARE_RESULTS Comparison;
    ULONG BytesWritten;
    ULONG CharsWritten;
    PCHAR Output;
    PCHAR OutputBuffer;
    PCBYTE Temp1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!!";
    PCBYTE Temp2 = "ABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCD";
    PCBYTE Temp3 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!!"
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!!";
    ULONG Lengths[] = {
        64,
        128,
        192,
        256,
        384,
        512,
        1024,
        2048,
        4096,
        0,
        16384,
        32768,
        65536,
        1 << 17,
        1 << 18,
        1 << 19,
        0
    };
    PULONG Length;

    ZeroStruct(HistogramA);
    ZeroStruct(HistogramB);

    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    ASSERT(OutputHandle);

    OldCodePage = GetConsoleCP();

    ASSERT(SetConsoleCP(20127));

    ASSERT(
        MakeRandomString(Rtl,
                         Allocator,
                         BufferSize,
                         &Buffer)
    );

    Success = CreateBuffer(Rtl, NULL, 1, 0, &OutputBufferSize, &OutputBuffer);
    ASSERT(Success);

    Output = OutputBuffer;

    String.Length = BufferSize;
    String.Hash = 0;
    String.Buffer = Buffer;

    QueryPerformanceFrequency(&Frequency);

    String.Length = 64;
    //String.Length = 64;

    CopyMemory(String.Buffer, Temp2, 64);
    //String.Buffer = (PBYTE)QuickLazy;

    Result = Histo1544(&String, &HistogramB);
    //Result = Histo1544v2(&String, &HistogramB);
    //Result = Histo1710(&String, &HistogramB);
    //Result = Histo1710v3(&String, &HistogramB);

    Result = Api->CreateHistogramAvx512AlignedAsm(&String,
                                                  &HistogramB);

    ASSERT(Result);


    Result = Api->CreateHistogramAvx2AlignedAsm(&String, &HistogramA);
    ASSERT(Result);

    Histogram1 = &HistogramA.Histogram1;
    Histogram2 = &HistogramB.Histogram1;

    Comparison = Api->CompareHistograms(Histogram1, Histogram2);
    SlowCompareHistogram(Histogram1, Histogram2, &Output);
    OUTPUT_FLUSH();

    return;

    Comparison = Api->CompareHistograms(Histogram1, Histogram2);
    ASSERT(Comparison == GenericEqual);

    return;

    INIT_TIMESTAMP(1, "CreateHistogramAvx2AlignedAsm  ");
    INIT_TIMESTAMP(2, "CreateHistogramAvx512AlignedAsm");

    OUTPUT_RAW("Name,Length,Iterations,Minimum,Maximum\n");

    Iterations = 1;
    Length = Lengths;

    do {
        String.Length = *Length;

        RESET_TIMESTAMP(1);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(1);
            Result = Api->CreateHistogramAvx2AlignedAsm(&String,
                                                        &HistogramB);
            END_TIMESTAMP(1);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(1, Length, Iterations);

        RESET_TIMESTAMP(2);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(2);
            Result = Api->CreateHistogramAvx512AlignedAsm(&String,
                                                          &HistogramB);
            END_TIMESTAMP(2);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(2, Length, Iterations);


        OUTPUT_FLUSH();
    } while (*(++Length));

    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    OUTPUT_FLUSH();

    ASSERT(SetConsoleCP(OldCodePage));

}

VOID
Scratch9(
    PRTL Rtl,
    PALLOCATOR Allocator,
    PDICTIONARY_FUNCTIONS Api
    )
{
    BOOL Success;
    ULONG Index;
    ULONG Iterations;
    ULONG OldCodePage;
    PBYTE Buffer;
    ULARGE_INTEGER BytesToWrite;
    LONG_STRING String;
    BOOLEAN Result;
    CHARACTER_HISTOGRAM_V4 HistogramA;
    CHARACTER_HISTOGRAM_V4 HistogramB;
    PCHARACTER_HISTOGRAM Histogram1;
    PCHARACTER_HISTOGRAM Histogram2;
    HANDLE OutputHandle;
    LARGE_INTEGER Frequency;
    TIMESTAMP Timestamp1;
    TIMESTAMP Timestamp2;
    TIMESTAMP Timestamp3;
    TIMESTAMP Timestamp4;
    TIMESTAMP Timestamp5;
    TIMESTAMP Timestamp6;
    ULONG BufferSize = 1 << 23;
    ULONGLONG OutputBufferSize;
    RTL_GENERIC_COMPARE_RESULTS Comparison;
    ULONG BytesWritten;
    ULONG CharsWritten;
    PCHAR Output;
    PCHAR OutputBuffer;
    ULONG Lengths[] = {
        192,
        384,
        64,
        128,
        256,
        512,
        1024,
        2048,
        4096,
        //0,
        16384,
        32768,
        65536,
        1 << 17,
        1 << 18,
        1 << 19,
        0
    };
    PULONG Length;

    ZeroStruct(HistogramA);
    ZeroStruct(HistogramB);

    Histogram1 = &HistogramA.Histogram1;
    Histogram2 = &HistogramB.Histogram1;

    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    ASSERT(OutputHandle);

    OldCodePage = GetConsoleCP();

    ASSERT(SetConsoleCP(20127));

    ASSERT(
        MakeRandomString(Rtl,
                         Allocator,
                         BufferSize,
                         &Buffer)
    );

    Success = CreateBuffer(Rtl, NULL, 1, 0, &OutputBufferSize, &OutputBuffer);
    ASSERT(Success);

    Output = OutputBuffer;

    String.Length = BufferSize;
    String.Hash = 0;
    String.Buffer = Buffer;

    if (0) {
        FillBufferWithBytes(Buffer,
                            AbcdRepeat,
                            //AlphabetRepeat,
                            BufferSize,
                            64);
    }

    QueryPerformanceFrequency(&Frequency);

    INIT_TIMESTAMP(1, "CreateHistogramAvx2AlignedAsm     ");
    INIT_TIMESTAMP(2, "CreateHistogramAvx512AlignedAsm   ");
    INIT_TIMESTAMP(3, "CreateHistogramAvx512AlignedAsm_v2");
    INIT_TIMESTAMP(4, "CreateHistogramAvx512AlignedAsm_v3");
    INIT_TIMESTAMP(5, "CreateHistogramAvx512AlignedAsm_v4");
    INIT_TIMESTAMP(6, "CreateHistogramAvx512AlignedAsm_v5");

    OUTPUT_RAW("Name,Length,Iterations,Minimum,Maximum\n");

    Iterations = 1000;
    Length = Lengths;

    String.Length = *Length;

    Result = Api->CreateHistogramAvx512AlignedAsm_v4(&String, &HistogramB);

    do {
        String.Length = *Length;

        ZeroStruct(HistogramA);
        Result = Api->CreateHistogram(&String, Histogram1);
        ASSERT(Result);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v2(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v3(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v4(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v5(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v5_2(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        Result = Api->CreateHistogramAvx2AlignedAsm_v5_3(&String, &HistogramB);
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);


        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx2AlignedAsm_v5_3_2(&String, &HistogramB));
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx2AlignedAsm_v5_3_3(&String, &HistogramB));
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx512AlignedAsm(&String, &HistogramB));
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx512AlignedAsm_v2(&String, &HistogramB));
        Comparison = Api->CompareHistograms(Histogram1, Histogram2);
        ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx512AlignedAsm_v3(&String, &HistogramB));
        COMPARE(Comparison, Histogram1, Histogram2, Output);
        //ASSERT(Comparison == GenericEqual);

        ZeroStruct(HistogramB);
        ASSERT(Api->CreateHistogramAvx512AlignedAsm_v4(&String, &HistogramB));
        COMPARE(Comparison, Histogram1, Histogram2, Output);

        //ZeroStruct(HistogramB);
        //ASSERT(Api->CreateHistogramAvx512AlignedAsm_v5(&String, &HistogramB));
        //COMPARE(Comparison, Histogram1, Histogram2, Output);

        RESET_TIMESTAMP(1);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(1);
            Result = Api->CreateHistogramAvx2AlignedAsm(&String,
                                                        &HistogramB);
            END_TIMESTAMP(1);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(1, Length, Iterations);

        RESET_TIMESTAMP(2);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(2);
            Result = Api->CreateHistogramAvx512AlignedAsm(&String,
                                                          &HistogramB);
            END_TIMESTAMP(2);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(2, Length, Iterations);

        RESET_TIMESTAMP(3);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(3);
            Result = Api->CreateHistogramAvx512AlignedAsm_v2(&String,
                                                             &HistogramB);
            END_TIMESTAMP(3);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(3, Length, Iterations);

        RESET_TIMESTAMP(4);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(4);
            Result = Api->CreateHistogramAvx512AlignedAsm_v3(&String,
                                                             &HistogramB);
            END_TIMESTAMP(4);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(4, Length, Iterations);

        RESET_TIMESTAMP(5);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(5);
            Result = Api->CreateHistogramAvx512AlignedAsm_v4(&String,
                                                             &HistogramB);
            END_TIMESTAMP(5);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(5, Length, Iterations);

        /*
        RESET_TIMESTAMP(6);
        for (Index = 0; Index < Iterations; Index++) {
            ZeroStruct(HistogramB);
            START_TIMESTAMP(6);
            Result = Api->CreateHistogramAvx512AlignedAsm_v5(&String,
                                                             &HistogramB);
            END_TIMESTAMP(6);
            ASSERT(Result);
        }
        FINISH_TIMESTAMP(6, Length, Iterations);
        */


        OUTPUT_FLUSH();
    } while (*(++Length));

    Allocator->FreePointer(Allocator, (PPVOID)&Buffer);

    OUTPUT_FLUSH();

    ASSERT(SetConsoleCP(OldCodePage));

}


extern
ULONGLONG
TestParams2(
    ULONG Param1,
    CHAR Param2,
    PVOID Param3
    );

VOID
Scratch8(
    VOID
    )
{
    //ULONGLONG Result;

    //Result = TestParams2(1, 'A', NULL);
}

extern
VOID
ScratchAvx1(VOID);


DECLSPEC_NORETURN
VOID
WINAPI
mainCRTStartup()
{
    LONG ExitCode = 0;
    LONG SizeOfRtl = sizeof(GlobalRtl);
    HMODULE RtlModule;
    RTL_BOOTSTRAP Bootstrap;
    HANDLE ProcessHandle;
    HANDLE ThreadHandle;
    SYSTEM_INFO SystemInfo;
    DWORD_PTR ProcessAffinityMask;
    DWORD_PTR SystemAffinityMask;
    DWORD_PTR OldThreadAffinityMask;
    DWORD_PTR AffinityMask;
    DWORD IdealProcessor;
    DWORD Result;

    if (!BootstrapRtl(&RtlModule, &Bootstrap)) {
        ExitCode = 1;
        goto Error;
    }

    if (!Bootstrap.InitializeHeapAllocator(&GlobalAllocator)) {
        ExitCode = 1;
        goto Error;
    }

    CHECKED_MSG(
        Bootstrap.InitializeRtl(&GlobalRtl, &SizeOfRtl),
        "InitializeRtl()"
    );

    Rtl = &GlobalRtl;
    Allocator = &GlobalAllocator;

    SetCSpecificHandler(Rtl->__C_specific_handler);

    //ASSERT(TestCreateBuffer(Rtl, CreateBuffer));

    CHECKED_MSG(
        LoadDictionaryModule(
            Rtl,
            &GlobalModule,
            &GlobalApi
        ),
        "LoadDictionaryModule"
    );

    Api = &GlobalApi;

    GetSystemInfo(&SystemInfo);
    IdealProcessor = SystemInfo.dwNumberOfProcessors - 1;

    ProcessHandle = GetCurrentProcess();
    ThreadHandle = GetCurrentThread();

    Result = SetThreadIdealProcessor(ThreadHandle, IdealProcessor);
    ASSERT(Result != (DWORD)-1);

    ASSERT(GetProcessAffinityMask(ProcessHandle,
                                  &ProcessAffinityMask,
                                  &SystemAffinityMask));

    AffinityMask = ((DWORD_PTR)1 << (DWORD_PTR)IdealProcessor);

    OldThreadAffinityMask = SetThreadAffinityMask(ThreadHandle, AffinityMask);
    ASSERT(OldThreadAffinityMask);

    ASSERT(SetThreadPriority(ThreadHandle, THREAD_PRIORITY_HIGHEST));

    // Scratch1();

    //Scratch2(Rtl, Allocator, Api);

    //Scratch4(Rtl, Allocator, Api);
    //ScratchAvx1();
    //Scratch8();
    //Scratch6(Rtl, Allocator, Api);
    //Scratch9(Rtl, Allocator, Api);
    Scratch5(Rtl, Allocator, Api);

Error:

    ExitProcess(ExitCode);
}

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
