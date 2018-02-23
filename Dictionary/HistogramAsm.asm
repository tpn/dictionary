        title "Histogram AVX Routines"
        option nokeyword:<Length>

;++
;
; Copyright (c) 2018 Trent Nelson <trent@trent.me>
;
; Module Name:
;
;   Histogram.asm
;
; Abstract:
;
;   This module implements various routines for constructing histograms.
;
;--

include ksamd64.inc

;
; Define the LONG_STRING structure used to encapsulate string information.
;

LONG_STRING struct
    Length  dd  ?
    Hash    dd  ?
    Buffer  dq  ?
LONG_STRING ends
PLONG_STRING typedef ptr LONG_STRING

;
; Define the CHARACTER_HISTOGRAM structure used to encapsulate the histogram.
;

CHARACTER_HISTOGRAM struct
    Count   dd 256 dup (?)
CHARACTER_HISTOGRAM ends
PCHARACTER_HISTOGRAM typedef ptr CHARACTER_HISTOGRAM

CHARACTER_HISTOGRAM_V4 struct
    Histogram1 CHARACTER_HISTOGRAM { }
    Histogram2 CHARACTER_HISTOGRAM { }
    Histogram3 CHARACTER_HISTOGRAM { }
    Histogram4 CHARACTER_HISTOGRAM { }
CHARACTER_HISTOGRAM_V4 ends
PCHARACTER_HISTOGRAM_V4 typedef ptr CHARACTER_HISTOGRAM_V4

;
; Define a Locals structure used for referencing our register homing space from
; rsp.
;

Locals struct
    ReturnAddress   dq  ?
    HomeRcx         dq  ?
    HomeRdx         dq  ?
    HomeR8          dq  ?
    HomeR9          dq  ?
Locals ends

;
; Define helper typedefs that are nicer to work with in assembly than their long
; uppercase name counterparts.
;

String typedef LONG_STRING
Histogram typedef CHARACTER_HISTOGRAM_V4

;++
;
; BOOLEAN
; CreateHistogramAvx2AlignedAsm(
;     _In_ PCLONG_STRING String,
;     _Inout_updates_bytes_(sizeof(*Histogram))
;         PCHARACTER_HISTOGRAM_V4 Histogram
;     );
;
; Routine Description:
;
;   This routine creates a histogram for a given string.
;
; Arguments:
;
;   String (rcx) - Supplies a pointer to a LONG_STRING structure that contains
;       the string for which a histogram is to be created.
;
;   Histogram (rdx) - Supplies an address that receives the histogram for the
;       given input string.
;
; Return Value:
;
;   TRUE on success, FALSE on failure.
;
;--

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm, _TEXT$00

;
; Clear return value (Success = FALSE).
;

        xor     rax, rax                                ; Clear rax.

;
; Validate parameters.
;

        test    rcx, rcx                                ; Is rcx NULL?
        jz      Cha99                                   ; Yes, abort.
        test    rdx, rdx                                ; Is rdx NULL?
        jz      Cha99                                   ; Yes, abort.

;
; Verify the string is at least 64 bytes long.
;
        mov     r9, 64                                  ; Initialize r9 to 64.
        cmp     String.Length[rcx], r9d                 ; Compare Length to 64.
        jl      Cha99                                   ; String is too short.

;
; Ensure the incoming string and histogram buffers are aligned to 32-byte
; boundaries.
;

        mov     r9, 31                                  ; Initialize r9 to 31.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Cha99                                   ; No, abort.

;
; Initialize loop variables.
;
;   rcx - Counter.
;
;   rdx - Base string buffer.
;
;   r8  - Base address of first histogram buffer.
;
;   r9  - Base address of second histogram buffer.
;
;   r10 - Byte counter.
;
;   r11 - Byte counter.
;

        mov     r8, rdx                             ; Load 1st histo buffer.
        lea     r9, Histogram.Histogram2[r8]        ; Load 2nd histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 5                              ; Divide by 32 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovntdqa       ymm0, ymmword ptr [rdx]     ; Load 32 bytes into ymm0.
        vextracti128    xmm2, ymm0, 1               ; Copy 16-31 bytes to xmm2.
        vmovdqa         xmm1, xmm0                  ; Duplicate xmm0 into xmm1.
        vmovdqa         xmm3, xmm2                  ; Duplicate xmm2 into xmm3.
        add             rdx, 32                     ; Increment pointer.

;
; Process bytes 0-15, registers xmm0 and xmm1.
;

        vpextrb     r10, xmm0, 0                        ; Extract byte 0.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 1                        ; Extract byte 1.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 2                        ; Extract byte 2.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 3                        ; Extract byte 3.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 4                        ; Extract byte 4.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 5                        ; Extract byte 5.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 6                        ; Extract byte 6.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 7                        ; Extract byte 7.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 8                        ; Extract byte 8.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 9                        ; Extract byte 9.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 10                       ; Extract byte 10.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 11                       ; Extract byte 11.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 12                       ; Extract byte 12.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 13                       ; Extract byte 13.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 14                       ; Extract byte 14.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 15                       ; Extract byte 15.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; Continue processing the second set of 16 bytes (16-31) via xmm2 and xmm3.
;

        vpextrb     r10, xmm2, 0                        ; Extract byte 16.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 1                        ; Extract byte 17.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 2                        ; Extract byte 18.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 3                        ; Extract byte 19.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 4                        ; Extract byte 20.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 5                        ; Extract byte 21.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 6                        ; Extract byte 22.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 7                        ; Extract byte 23.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 8                        ; Extract byte 24.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 9                        ; Extract byte 25.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 10                       ; Extract byte 26.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 11                       ; Extract byte 27.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 12                       ; Extract byte 28.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 13                       ; Extract byte 29.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 14                       ; Extract byte 30.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 15                       ; Extract byte 31.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; End of loop.  Update loop counter and determine if we're finished.
;

        sub         ecx, 1                              ; Decrement counter.
        jnz         Cha50                               ; Continue loop if != 0.

;
; We've finished creating the histogram.  Merge the two histograms 64 bytes at
; a time using YMM registers.
;

        mov         ecx, 16                             ; Initialize counter.

        align 16

Cha75:  vmovntdqa   ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vmovntdqa   ymm1, ymmword ptr [r8+rax+20h]  ; Load 1st histo 32-63.
        vmovntdqa   ymm2, ymmword ptr [r9+rax]      ; Load 2nd histo  0-31.
        vmovntdqa   ymm3, ymmword ptr [r9+rax+20h]  ; Load 2nd histo 32-63.

        vpaddd      ymm4, ymm0, ymm2                ; Add  0-31 counts.
        vpaddd      ymm5, ymm1, ymm3                ; Add 32-63 counts.

        vmovntdq    ymmword ptr [r8+rax], ymm4      ; Save counts for  0-31.
        vmovntdq    ymmword ptr [r8+rax+20h], ymm5  ; Save counts for 32-63.

        add         rax, 40h                        ; Advance to next 64 bytes.
        sub         ecx, 1                          ; Decrement loop counter.
        jnz         short Cha75                     ; Continue if != 0.

;
; Indicate success and return.
;

Cha98:  mov rax, 1

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm, _TEXT$00

;++
;
; BOOLEAN
; CreateHistogram(
;     _In_ _Const_ PCLONG_STRING String,
;     _Inout_updates_bytes_(sizeof(*Histogram)) PCHARACTER_HISTOGRAM Histogram,
;     _Out_ PULONG HistogramHash
;     );
;
; Routine Description:
;
;   This routine creates a histogram for a given string.
;
; Arguments:
;
;   String (rcx) - Supplies a pointer to a LONG_STRING structure that contains
;       the string for which a histogram is to be created.
;
;   Histogram (rdx) - Supplies an address that receives the histogram for the
;       given input string.
;
;   Hash (r8) - Supplies the address of a variable that receives the CRC32 hash
;       value for the histogram.
;
; Return Value:
;
;   TRUE on success, FALSE on failure.
;
;--

        LEAF_ENTRY_ARG3 CreateHistogramOld, _TEXT$00, String, Histogram, Hash

;
; Home our parameters.
;

        mov     Locals.HomeRcx[rsp], rcx                ; Home rcx.
        mov     Locals.HomeRdx[rsp], rdx                ; Home rdx.
        mov     Locals.HomeR8[rsp], r8                  ; Home r8.
        mov     Locals.HomeR9[rsp], r9                  ; Home r9.

;
; Clear return value (Success = FALSE).
;

        xor     rax, rax                                ; Clear rax.

;
; Validate parameters.
;

        test    rcx, rcx                                ; Is rcx NULL?
        jz      Chz99                                   ; Yes, abort.
        test    rdx, rdx                                ; Is rdx NULL?
        jz      Chz99                                   ; Yes, abort.
        test    r8, r8                                  ; Is r8 NULL?
        jz      Chz99                                   ; Yes, abort.

;
; Verify the string is at least 32 bytes long.
;
        mov     r9, 32                                  ; Initialize r9 to 32.
        cmp     String.Length[rcx], r9d                 ; Compare Length to 32.
        jl      Chz99                                   ; String is too short.

;
; Ensure the incoming string and histogram buffers are aligned to 32-byte
; boundaries.
;

        dec     r9                                      ; r9 is now 31.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Chz99                                   ; No, abort.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Chz99                                   ; No, abort.

;
; Initialize loop variables.
;
;   rcx - Base string buffer.
;
;   rdx - Base histogram buffer.
;
;   r8  - Base of second histogram buffer.  This is 256 bytes from the start
;         of the first histogram.
;
;   r9  - Counter.
;

        mov     rcx, String.Buffer[rcx]                 ; Load string buffer.



;
; Indicate success and return.
;

        add rax, 1

Chz99:  ret

        LEAF_END CreateHistogramOld, _TEXT$00



; vim:set tw=80 ts=8 sw=4 sts=4 et syntax=masm fo=croql comments=\:;           :

end
