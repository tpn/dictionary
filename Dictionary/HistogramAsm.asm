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
; Define constants.
;

OP_EQ   equ 0
OP_NEQ  equ 4

;
; Define constant variables.
;

ZMM_ALIGN equ 64
YMM_ALIGN equ 32
XMM_ALIGN equ 16

PVOID typedef ptr

_DATA$00 SEGMENT PAGE 'DATA'

        align   ZMM_ALIGN
QuickLazy       db      "The quick brown fox jumps over the lazy dog.  "

        align   ZMM_ALIGN
        public  Test1
Test1           db      "ABACAEEFGIHIJJJKLMNDOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!!"

        align   ZMM_ALIGN
        public  Shuffle1
Shuffle1        db  64  dup (02h, 00h, 00h, 00h, 60 dup (00h))

        align   ZMM_ALIGN
        public  AllOnes
AllOnes         dd  16  dup (1)

        align   ZMM_ALIGN
        public  AllNegativeOnes
AllNegativeOnes dd  16  dup (-1)

        align   ZMM_ALIGN
        public  AllBinsMinusOne
AllBinsMinusOne dd  16  dup (254)

        align   ZMM_ALIGN
        public  AllThirtyOne
AllThirtyOne    dd  16  dup (31)

_DATA$00 ends

IACA_VC_START macro Name

        mov     byte ptr gs:[06fh], 06fh

        endm

IACA_VC_END macro Name

        mov     byte ptr gs:[0deh], 0deh

        endm

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
    Temp dq ?

    ;
    ; Define non-volatile register storage.
    ;

    union
        FirstNvRegister     dq      ?
        SavedRbp            dq      ?
    ends

    SavedRbx                dq      ?
    SavedRdi                dq      ?
    SavedRsi                dq      ?
    SavedR12                dq      ?
    SavedR13                dq      ?
    SavedR14                dq      ?

    SavedXmm6               XMMWORD { }
    SavedXmm7               XMMWORD { }
    SavedXmm8               XMMWORD { }
    SavedXmm9               XMMWORD { }
    SavedXmm10              XMMWORD { }
    SavedXmm11              XMMWORD { }
    SavedXmm12              XMMWORD { }
    SavedXmm13              XMMWORD { }
    SavedXmm14              XMMWORD { }
    SavedXmm15              XMMWORD { }

    ;
    ; Stash R15 after the return address to ensure the XMM register space
    ; is aligned on a 16 byte boundary, as we use movdqa (i.e. aligned move)
    ; which will fault if we're only 8 byte aligned.
    ;

    SavedR15                dq      ?

    ReturnAddress           dq  ?
    HomeRcx                 dq  ?
    HomeRdx                 dq  ?
    HomeR8                  dq  ?
    HomeR9                  dq  ?
Locals ends

;
; Exclude the return address onward from the frame calculation size.
;

LOCALS_SIZE  equ ((sizeof Locals) + (Locals.ReturnAddress - (sizeof Locals)))


Locals4 struct
    Temp dq ?

    ;
    ; Define non-volatile register storage.
    ;

    union
        FirstNvRegister     dq      ?
        SavedR12            dq      ?
    ends

    SavedR13                dq      ?
    SavedR14                dq      ?
    SavedR15                dq      ?

    ReturnAddress           dq  ?
    HomeRcx                 dq  ?
    HomeRdx                 dq  ?
    HomeR8                  dq  ?
    HomeR9                  dq  ?
Locals4 ends

;
; Exclude the return address onward from the frame calculation size.
;

LOCALS4_SIZE  equ ((sizeof Locals4) + (Locals4.ReturnAddress - (sizeof Locals4)))


;
; Define helper typedefs that are nicer to work with in assembly than their long
; uppercase name counterparts.
;

String typedef LONG_STRING
Histogram typedef CHARACTER_HISTOGRAM_V4

;++
;
; BOOLEAN
; CreateHistogramAlignedAsm(
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

        LEAF_ENTRY CreateHistogramAlignedAsm, _TEXT$00

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
;   r10 - Loop counter (string length shifted right 3 times).
;

        mov     r8, rdx                             ; Load 1st histo buffer.
        lea     r9, Histogram.Histogram2[r8]        ; Load 2nd histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     r10d, String.Length[rcx]            ; Load string length.
        shr     r10, 3                              ; Divide by 8 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load 8 bytes into rax and advance the buffer pointer rdx.
;

Cha50:  mov         rax, [rdx]                  ; Load 8 bytes into rax.
        add         rdx, 8                      ; Bump pointer.

;
; Process bytes 1 and 2.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        shr         rax, 16                     ; Shift to the next two bytes.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for upper byte.

;
; Process bytes 3 and 4.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        shr         rax, 16                     ; Shift to the next two bytes.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for upper byte.

;
; Process bytes 5 and 6.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        shr         rax, 16                     ; Shift to the next two bytes.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for upper byte.

;
; Process bytes 7 and 8.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for upper byte.

;
; End of loop.  Update loop counter and determine if we're finished.
;

        sub         r10, 1                      ; Decrement loop counter.
        jnz         short Cha50                 ; If nz, continue loop.

;
; We've finished creating the histogram.  Indicate success and return.
;

Cha98:  mov rax, 1
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAlignedAsm, _TEXT$00

;
; Alternate version of routine above that uses two histogram buffers and sums
; them at the end.
;

        LEAF_ENTRY CreateHistogramAlignedAsm_v2, _TEXT$00

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
;   r10 - Loop counter (string length shifted right 3 times).
;

        mov     r8, rdx                             ; Load 1st histo buffer.
        lea     r9, Histogram.Histogram2[r8]        ; Load 2nd histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     r10d, String.Length[rcx]            ; Load string length.
        shr     r10, 3                              ; Divide by 8 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load 8 bytes into rax and advance the buffer pointer rdx.
;

Cha50:  mov         rax, [rdx]                  ; Load 8 bytes into rax.
        add         rdx, 8                      ; Bump pointer.

;
; Process bytes 1 and 2.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        shr         rax, 16                     ; Shift to the next two bytes.
        add         dword ptr [r9 + rcx * 4], 1 ; Update count for upper byte.

;
; Process bytes 3 and 4.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        shr         rax, 16                     ; Shift to the next two bytes.
        add         dword ptr [r9 + rcx * 4], 1 ; Update count for upper byte.

;
; Process bytes 5 and 6.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        shr         rax, 16                     ; Shift to the next two bytes.
        add         dword ptr [r9 + rcx * 4], 1 ; Update count for upper byte.

;
; Process bytes 7 and 8.
;

        movzx       rcx, al                     ; Isolate lower byte.
        add         dword ptr [r8 + rcx * 4], 1 ; Update count for lower.
        movzx       ecx, ah                     ; Isolate upper byte.
        shr         rax, 16                     ; Shift to the next two bytes.
        add         dword ptr [r9 + rcx * 4], 1 ; Update count for upper byte.

;
; End of loop.  Update loop counter and determine if we're finished.
;

        sub         r10, 1                      ; Decrement loop counter.
        jnz         short Cha50                 ; If nz, continue loop.

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
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAlignedAsm_v2, _TEXT$00

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

        ;IACA_VC_START


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
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm_v2, _TEXT$00

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
        shr     ecx, 5                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovntdqa       xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovdqa         xmm1, xmm0                  ; Duplicate xmm0 into xmm1.
        vmovntdqa       xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        vmovdqa         xmm3, xmm2                  ; Duplicate xmm2 into xmm3.
        add             rdx, 20h                    ; Increment pointer.

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
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm_v2, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm_v3, _TEXT$00

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
        shr     ecx, 4                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load 16 bytes into xmm0.
;

Cha50:  vmovntdqa       xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovdqa         xmm1, xmm0                  ; Duplicate xmm0 into xmm1.
        ;vmovntdqa       xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        ;vmovdqa         xmm3, xmm2                  ; Duplicate xmm2 into xmm3.
        add             rdx, 10h                    ; Increment pointer.
        prefetchnta     xmmword ptr [rdx]           ; Prefetch next chunk.

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
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm_v3, _TEXT$00


        NESTED_ENTRY CreateHistogramAvx2AlignedAsm_v4, _TEXT$00

        alloc_stack LOCALS4_SIZE

        save_reg    r12, Locals4.SavedR12        ; Save non-volatile r12.
        save_reg    r13, Locals4.SavedR13        ; Save non-volatile r13.
        save_reg    r14, Locals4.SavedR14        ; Save non-volatile r14.
        save_reg    r15, Locals4.SavedR15        ; Save non-volatile r15.

        END_PROLOGUE


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
        lea     r12, Histogram.Histogram3[r8]       ; Load 3rd histo buffer.
        lea     r13, Histogram.Histogram4[r8]       ; Load 4th histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 5                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovntdqa       xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovdqa         xmm1, xmm0                  ; Duplicate xmm0 into xmm1.
        vmovntdqa       xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        vmovdqa         xmm3, xmm2                  ; Duplicate xmm2 into xmm3.
        add             rdx, 20h                    ; Increment pointer.

;
; Process bytes 0-15, registers xmm0 and xmm1.
;

        vpextrb     r10, xmm0, 0                        ; Extract byte 0.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 1                        ; Extract byte 1.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm0, 2                        ; Extract byte 2.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm1, 3                        ; Extract byte 3.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

        vpextrb     r10, xmm0, 4                        ; Extract byte 4.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 5                        ; Extract byte 5.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm0, 6                        ; Extract byte 6.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm1, 7                        ; Extract byte 7.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

        vpextrb     r10, xmm0, 8                        ; Extract byte 8.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 9                        ; Extract byte 9.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm0, 10                       ; Extract byte 10.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm1, 11                       ; Extract byte 11.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

        vpextrb     r10, xmm0, 12                       ; Extract byte 12.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm1, 13                       ; Extract byte 13.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm0, 14                       ; Extract byte 14.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm1, 15                       ; Extract byte 15.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

;
; Continue processing the second set of 16 bytes (16-31) via xmm2 and xmm3.
;

        vpextrb     r10, xmm2, 0                        ; Extract byte 16.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 1                        ; Extract byte 17.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm2, 2                        ; Extract byte 18.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm3, 3                        ; Extract byte 19.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

        vpextrb     r10, xmm2, 4                        ; Extract byte 20.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 5                        ; Extract byte 21.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm2, 6                        ; Extract byte 22.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm3, 7                        ; Extract byte 23.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

        vpextrb     r10, xmm2, 8                        ; Extract byte 24.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 9                        ; Extract byte 25.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm2, 10                       ; Extract byte 26.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm3, 11                       ; Extract byte 27.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

        vpextrb     r10, xmm2, 12                       ; Extract byte 28.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm3, 13                       ; Extract byte 29.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r14, xmm2, 14                       ; Extract byte 30.
        add         dword ptr [r12 + r14 * 4], 1        ; Update count.

        vpextrb     r15, xmm3, 15                       ; Extract byte 31.
        add         dword ptr [r13 + r15 * 4], 1        ; Update count.

;
; End of loop.  Update loop counter and determine if we're finished.
;

        sub         ecx, 1                              ; Decrement counter.
        jnz         Cha50                               ; Continue loop if != 0.

;
; We've finished creating the histogram.  Merge the four histograms 64 bytes at
; a time using YMM registers.
;

        mov         ecx, 16                             ; Initialize counter.

        align 16

Cha75:  vmovntdqa   ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vmovntdqa   ymm2, ymmword ptr [r9+rax]      ; Load 2nd histo  0-31.
        vpaddd      ymm4, ymm0, ymm2                ; Add  0-31 counts.

        vmovntdqa   ymm1, ymmword ptr [r8+rax+20h]  ; Load 1st histo 32-63.
        vmovntdqa   ymm3, ymmword ptr [r9+rax+20h]  ; Load 2nd histo 32-63.
        vpaddd      ymm5, ymm1, ymm3                ; Add 32-63 counts.

        vmovntdqa   ymm0, ymmword ptr [r12+rax]     ; Load 3rd histo  0-31.
        vmovntdqa   ymm2, ymmword ptr [r13+rax]     ; Load 4th histo  0-31.
        vpaddd      ymm0, ymm0, ymm2                ; Add 3-4 0-31.

        vmovntdqa   ymm1, ymmword ptr [r12+rax+20h] ; Load 3rd histo 32-63.
        vmovntdqa   ymm3, ymmword ptr [r13+rax+20h] ; Load 4th histo 32-63.
        vpaddd      ymm1, ymm1, ymm3                ; Add 3-4 32-64.

        vpaddd      ymm4, ymm4, ymm0                ; Merge 1-2 & 3-4 for 0-31.
        vmovntdq    ymmword ptr [r8+rax], ymm4      ; Save counts for  0-31.

        vpaddd      ymm5, ymm5, ymm1                ; Merge 1-2 & 3-4 for 32-63.
        vmovntdq    ymmword ptr [r8+rax+20h], ymm5  ; Save counts for 32-63.

        add         rax, 40h                        ; Advance to next 64 bytes.
        sub         ecx, 1                          ; Decrement loop counter.
        jnz         short Cha75                     ; Continue if != 0.

;
; Indicate success and return.
;

Cha98:  mov rax, 1
        ;IACA_VC_END

Cha99:
        mov             r12,   Locals4.SavedR12[rsp]
        mov             r13,   Locals4.SavedR13[rsp]
        mov             r14,   Locals4.SavedR14[rsp]
        mov             r15,   Locals4.SavedR15[rsp]

        add             rsp, LOCALS4_SIZE
        ret

        NESTED_END CreateHistogramAvx2AlignedAsm_v4, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm_v5, _TEXT$00

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
        shr     ecx, 5                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovntdqa       xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovntdqa       xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        add             rdx, 20h                    ; Increment pointer.

;
; Process bytes 0-15, registers xmm0 and xmm1.
;

        vpextrb     r10, xmm0, 0                        ; Extract byte 0.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 1                        ; Extract byte 1.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 2                        ; Extract byte 2.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 3                        ; Extract byte 3.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 4                        ; Extract byte 4.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 5                        ; Extract byte 5.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 6                        ; Extract byte 6.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 7                        ; Extract byte 7.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 8                        ; Extract byte 8.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 9                        ; Extract byte 9.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 10                       ; Extract byte 10.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 11                       ; Extract byte 11.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 12                       ; Extract byte 12.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 13                       ; Extract byte 13.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 14                       ; Extract byte 14.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 15                       ; Extract byte 15.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; Continue processing the second set of 16 bytes (16-31) via xmm2 and xmm3.
;

        vpextrb     r10, xmm2, 0                        ; Extract byte 16.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 1                        ; Extract byte 17.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 2                        ; Extract byte 18.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 3                        ; Extract byte 19.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 4                        ; Extract byte 20.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 5                        ; Extract byte 21.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 6                        ; Extract byte 22.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 7                        ; Extract byte 23.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 8                        ; Extract byte 24.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 9                        ; Extract byte 25.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 10                       ; Extract byte 26.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 11                       ; Extract byte 27.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 12                       ; Extract byte 28.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 13                       ; Extract byte 29.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 14                       ; Extract byte 30.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 15                       ; Extract byte 31.
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
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm_v5, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm_v5_2, _TEXT$00

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
        shr     ecx, 5                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovdqa         xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovdqa         xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        add             rdx, 20h                    ; Increment pointer.

;
; Process bytes 0-15, registers xmm0 and xmm1.
;

        vpextrb     r10, xmm0, 0                        ; Extract byte 0.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 1                        ; Extract byte 1.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 2                        ; Extract byte 2.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 3                        ; Extract byte 3.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 4                        ; Extract byte 4.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 5                        ; Extract byte 5.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 6                        ; Extract byte 6.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 7                        ; Extract byte 7.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 8                        ; Extract byte 8.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 9                        ; Extract byte 9.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 10                       ; Extract byte 10.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 11                       ; Extract byte 11.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 12                       ; Extract byte 12.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 13                       ; Extract byte 13.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 14                       ; Extract byte 14.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 15                       ; Extract byte 15.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; Continue processing the second set of 16 bytes (16-31) via xmm2 and xmm3.
;

        vpextrb     r10, xmm2, 0                        ; Extract byte 16.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 1                        ; Extract byte 17.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 2                        ; Extract byte 18.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 3                        ; Extract byte 19.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 4                        ; Extract byte 20.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 5                        ; Extract byte 21.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 6                        ; Extract byte 22.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 7                        ; Extract byte 23.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 8                        ; Extract byte 24.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 9                        ; Extract byte 25.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 10                       ; Extract byte 26.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 11                       ; Extract byte 27.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 12                       ; Extract byte 28.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 13                       ; Extract byte 29.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 14                       ; Extract byte 30.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 15                       ; Extract byte 31.
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

Cha75:  vmovdqa   ymm0, ymmword ptr [r8+rax]        ; Load 1st histo  0-31.
        vmovdqa   ymm1, ymmword ptr [r8+rax+20h]    ; Load 1st histo 32-63.
        vmovdqa   ymm2, ymmword ptr [r9+rax]        ; Load 2nd histo  0-31.
        vmovdqa   ymm3, ymmword ptr [r9+rax+20h]    ; Load 2nd histo 32-63.

        vpaddd    ymm4, ymm0, ymm2                  ; Add  0-31 counts.
        vpaddd    ymm5, ymm1, ymm3                  ; Add 32-63 counts.

        vmovdqa   ymmword ptr [r8+rax], ymm4        ; Save counts for  0-31.
        vmovdqa   ymmword ptr [r8+rax+20h], ymm5    ; Save counts for 32-63.

        add         rax, 40h                        ; Advance to next 64 bytes.
        sub         ecx, 1                          ; Decrement loop counter.
        jnz         short Cha75                     ; Continue if != 0.

;
; Indicate success and return.
;

Cha98:  mov rax, 1
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm_v5_2, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm_v5_3, _TEXT$00

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
        shr     ecx, 5                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovdqa         xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovdqa         xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        add             rdx, 20h                    ; Increment pointer.

;
; Process bytes 0-15, registers xmm0 and xmm1.
;

        vpextrb     r10, xmm0, 0                        ; Extract byte 0.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 1                        ; Extract byte 1.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 2                        ; Extract byte 2.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 3                        ; Extract byte 3.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 4                        ; Extract byte 4.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 5                        ; Extract byte 5.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 6                        ; Extract byte 6.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 7                        ; Extract byte 7.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 8                        ; Extract byte 8.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 9                        ; Extract byte 9.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 10                       ; Extract byte 10.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 11                       ; Extract byte 11.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 12                       ; Extract byte 12.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 13                       ; Extract byte 13.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 14                       ; Extract byte 14.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 15                       ; Extract byte 15.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; Continue processing the second set of 16 bytes (16-31) via xmm2 and xmm3.
;

        vpextrb     r10, xmm2, 0                        ; Extract byte 16.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 1                        ; Extract byte 17.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 2                        ; Extract byte 18.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 3                        ; Extract byte 19.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 4                        ; Extract byte 20.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 5                        ; Extract byte 21.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 6                        ; Extract byte 22.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 7                        ; Extract byte 23.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 8                        ; Extract byte 24.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 9                        ; Extract byte 25.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 10                       ; Extract byte 26.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 11                       ; Extract byte 27.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 12                       ; Extract byte 28.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 13                       ; Extract byte 29.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 14                       ; Extract byte 30.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 15                       ; Extract byte 31.
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
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm_v5_3, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm_v5_3_2, _TEXT$00

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
        lea     r8, Histogram.Histogram3[r8]        ; Load 3rd histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 5                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovdqa         xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovdqa         xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        add             rdx, 20h                    ; Increment pointer.

;
; Process bytes 0-15, registers xmm0 and xmm1.
;

        vpextrb     r10, xmm0, 0                        ; Extract byte 0.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 1                        ; Extract byte 1.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 2                        ; Extract byte 2.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 3                        ; Extract byte 3.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 4                        ; Extract byte 4.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 5                        ; Extract byte 5.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 6                        ; Extract byte 6.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 7                        ; Extract byte 7.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 8                        ; Extract byte 8.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 9                        ; Extract byte 9.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 10                       ; Extract byte 10.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 11                       ; Extract byte 11.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 12                       ; Extract byte 12.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 13                       ; Extract byte 13.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 14                       ; Extract byte 14.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 15                       ; Extract byte 15.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; Continue processing the second set of 16 bytes (16-31) via xmm2 and xmm3.
;

        vpextrb     r10, xmm2, 0                        ; Extract byte 16.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 1                        ; Extract byte 17.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 2                        ; Extract byte 18.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 3                        ; Extract byte 19.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 4                        ; Extract byte 20.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 5                        ; Extract byte 21.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 6                        ; Extract byte 22.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 7                        ; Extract byte 23.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 8                        ; Extract byte 24.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 9                        ; Extract byte 25.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 10                       ; Extract byte 26.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 11                       ; Extract byte 27.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 12                       ; Extract byte 28.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 13                       ; Extract byte 29.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 14                       ; Extract byte 30.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 15                       ; Extract byte 31.
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
        lea         r10, [r9 - size CHARACTER_HISTOGRAM]; Copy 2nd histo addr.

        align 16

Cha75:  vmovntdqa   ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vmovntdqa   ymm1, ymmword ptr [r8+rax+20h]  ; Load 1st histo 32-63.

        vmovntdqa   ymm2, ymmword ptr [r9+rax]      ; Load 2nd histo  0-31.
        vmovntdqa   ymm3, ymmword ptr [r9+rax+20h]  ; Load 2nd histo 32-63.

        prefetchw   [r10+rax]

        vpaddd      ymm4, ymm0, ymm2                ; Add  0-31 counts.
        vpaddd      ymm5, ymm1, ymm3                ; Add 32-63 counts.

        vmovntdq    ymmword ptr [r10+rax], ymm4     ; Save counts for  0-31.
        vmovntdq    ymmword ptr [r10+rax+20h], ymm5 ; Save counts for 32-63.

        add         rax, 40h                        ; Advance to next 64 bytes.
        sub         ecx, 1                          ; Decrement loop counter.
        jnz         short Cha75                     ; Continue if != 0.

;
; Indicate success and return.
;

Cha98:  mov rax, 1
        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm_v5_3_2, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx2AlignedAsm_v5_3_3, _TEXT$00

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
        shr     ecx, 5                              ; Divide by 16 to get loop
                                                    ; iterations.

;
; Top of the histogram loop.
;

        align 16

        ;IACA_VC_START


;
; Load the first 32 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Cha50:  vmovdqa         xmm0, xmmword ptr [rdx]     ; Load 16 bytes into xmm0.
        vmovdqa         xmm2, xmmword ptr [rdx+10h] ; Load 16 bytes into xmm2.
        add             rdx, 20h                    ; Increment pointer.

;
; Process bytes 0-15, registers xmm0 and xmm1.
;

        vpextrb     r10, xmm0, 0                        ; Extract byte 0.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 1                        ; Extract byte 1.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 2                        ; Extract byte 2.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 3                        ; Extract byte 3.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 4                        ; Extract byte 4.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 5                        ; Extract byte 5.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 6                        ; Extract byte 6.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 7                        ; Extract byte 7.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 8                        ; Extract byte 8.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 9                        ; Extract byte 9.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 10                       ; Extract byte 10.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 11                       ; Extract byte 11.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 12                       ; Extract byte 12.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 13                       ; Extract byte 13.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm0, 14                       ; Extract byte 14.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm0, 15                       ; Extract byte 15.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; Continue processing the second set of 16 bytes (16-31) via xmm2 and xmm3.
;

        vpextrb     r10, xmm2, 0                        ; Extract byte 16.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 1                        ; Extract byte 17.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 2                        ; Extract byte 18.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 3                        ; Extract byte 19.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 4                        ; Extract byte 20.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 5                        ; Extract byte 21.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 6                        ; Extract byte 22.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 7                        ; Extract byte 23.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 8                        ; Extract byte 24.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 9                        ; Extract byte 25.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 10                       ; Extract byte 26.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 11                       ; Extract byte 27.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 12                       ; Extract byte 28.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 13                       ; Extract byte 29.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

        vpextrb     r10, xmm2, 14                       ; Extract byte 30.
        add         dword ptr [r8 + r10 * 4], 1         ; Update count.

        vpextrb     r11, xmm2, 15                       ; Extract byte 31.
        add         dword ptr [r9 + r11 * 4], 1         ; Update count.

;
; End of loop.  Update loop counter and determine if we're finished.
;

        sub         ecx, 1                              ; Decrement counter.
        jnz         Cha50                               ; Continue loop if != 0.

;
; We've finished creating the histogram.  Merge the two histograms 128 bytes at
; a time using YMM registers.
;

        mov         ecx, 8                             ; Initialize counter.
        mov         rdx, 80h

        align 16

Cha75:  vmovdqa     ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rax]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rax], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rax+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rax+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rax+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rax+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rax+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rax+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rax+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rax+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rax+60h], ymm3       ; Save     96-127.

        add         rax, 100h

        vmovdqa     ymm0, ymmword ptr [r8+rdx]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rdx]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rdx], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rdx+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rdx+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rdx+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rdx+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rdx+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rdx+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rdx+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rdx+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rdx+60h], ymm3       ; Save     96-127.

        add         rdx, 100h

;
; 256 bytes processed.
;

        vmovdqa     ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rax]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rax], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rax+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rax+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rax+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rax+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rax+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rax+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rax+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rax+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rax+60h], ymm3       ; Save     96-127.

        add         rax, 100h

        vmovdqa     ymm0, ymmword ptr [r8+rdx]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rdx]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rdx], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rdx+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rdx+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rdx+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rdx+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rdx+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rdx+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rdx+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rdx+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rdx+60h], ymm3       ; Save     96-127.

        add         rdx, 100h

;
; 512 bytes processed.
;

        vmovdqa     ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rax]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rax], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rax+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rax+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rax+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rax+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rax+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rax+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rax+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rax+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rax+60h], ymm3       ; Save     96-127.

        add         rax, 100h

        vmovdqa     ymm0, ymmword ptr [r8+rdx]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rdx]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rdx], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rdx+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rdx+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rdx+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rdx+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rdx+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rdx+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rdx+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rdx+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rdx+60h], ymm3       ; Save     96-127.

        add         rdx, 100h

;
; 768 bytes processed.
;

        vmovdqa     ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rax]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rax], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rax+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rax+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rax+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rax+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rax+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rax+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rax+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rax+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rax+60h], ymm3       ; Save     96-127.

;
; Set rax to 1.
;

        shr         rax, 9

        vmovdqa     ymm0, ymmword ptr [r8+rdx]      ; Load 1st histo  0-31.
        vpaddd      ymm0, ymm0, ymmword ptr [r9+rdx]; Add  2nd histo  0-31.
        vmovntdq    ymmword ptr [r8+rdx], ymm0      ; Save counts for  0-31.

        vmovdqa     ymm1, ymmword ptr [r8+rdx+20h]       ; Load 1st 32-63.
        vpaddd      ymm1, ymm1, ymmword ptr [r9+rdx+20h] ; Add  2nd 32-63.
        vmovntdq    ymmword ptr [r8+rdx+20h], ymm1       ; Save 32-63.

        vmovdqa     ymm2, ymmword ptr [r8+rdx+40h]       ; Load 1st 64-95.
        vpaddd      ymm2, ymm2, ymmword ptr [r9+rdx+40h] ; Add  2nd 64-95.
        vmovntdq    ymmword ptr [r8+rdx+40h], ymm2       ; Save 64-95.

        vmovdqa     ymm3, ymmword ptr [r8+rdx+60h]       ; Load 1st 96-127.
        vpaddd      ymm3, ymm3, ymmword ptr [r9+rdx+60h] ; Add  2nd 96-127.
        vmovntdq    ymmword ptr [r8+rdx+60h], ymm3       ; Save     96-127.

        ;IACA_VC_END

Cha99:  ret

        LEAF_END CreateHistogramAvx2AlignedAsm_v5_3_3, _TEXT$00


;++
;
; BOOLEAN
; CreateHistogramAvx512AlignedAsm(
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

        NESTED_ENTRY CreateHistogramAvx512AlignedAsm, _TEXT$00

;
; Begin prologue.  Allocate stack space and save non-volatile registers.
;

        alloc_stack LOCALS_SIZE

        save_reg    rbp, Locals.SavedRbp        ; Save non-volatile rbp.
        save_reg    rbx, Locals.SavedRbx        ; Save non-volatile rbx.
        save_reg    rdi, Locals.SavedRdi        ; Save non-volatile rdi.
        save_reg    rsi, Locals.SavedRsi        ; Save non-volatile rsi.
        save_reg    r12, Locals.SavedR12        ; Save non-volatile r12.
        save_reg    r13, Locals.SavedR13        ; Save non-volatile r13.
        save_reg    r14, Locals.SavedR14        ; Save non-volatile r14.
        save_reg    r15, Locals.SavedR15        ; Save non-volatile r15.

        save_xmm128 xmm6, Locals.SavedXmm6      ; Save non-volatile xmm6.
        save_xmm128 xmm7, Locals.SavedXmm7      ; Save non-volatile xmm7.
        save_xmm128 xmm8, Locals.SavedXmm8      ; Save non-volatile xmm8.
        save_xmm128 xmm9, Locals.SavedXmm9      ; Save non-volatile xmm9.
        save_xmm128 xmm10, Locals.SavedXmm10    ; Save non-volatile xmm10.
        save_xmm128 xmm11, Locals.SavedXmm11    ; Save non-volatile xmm11.
        save_xmm128 xmm12, Locals.SavedXmm12    ; Save non-volatile xmm12.
        save_xmm128 xmm13, Locals.SavedXmm13    ; Save non-volatile xmm13.
        save_xmm128 xmm14, Locals.SavedXmm14    ; Save non-volatile xmm14.
        save_xmm128 xmm15, Locals.SavedXmm15    ; Save non-volatile xmm15.

        END_PROLOGUE


;
; Clear return value (Success = FALSE).
;

        ;IACA_VC_START

        xor     rax, rax                                ; Clear rax.

;
; Validate parameters.
;

        test    rcx, rcx                                ; Is rcx NULL?
        jz      Chb99                                   ; Yes, abort.
        test    rdx, rdx                                ; Is rdx NULL?
        jz      Chb99                                   ; Yes, abort.

;
; Verify the string is at least 64 bytes long.
;
        mov     r9, 64                                  ; Initialize r9 to 64.
        cmp     String.Length[rcx], r9d                 ; Compare Length to 64.
        jl      Chb99                                   ; String is too short.

;
; Ensure the incoming string and histogram buffers are aligned to 32-byte
; boundaries.
;

        mov     r9, 31                                  ; Initialize r9 to 31.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Chb99                                   ; No, abort.

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
;   r10 - Base address of third histogram buffer.
;
;   r11 - Base address of fourth histogram buffer.
;

        mov     r8,  rdx                            ; Load 1st histo buffer.
        lea     r9,  Histogram.Histogram2[r8]       ; Load 2nd histo buffer.
        lea     r10, Histogram.Histogram3[r8]       ; Load 3rd histo buffer.
        lea     r11, Histogram.Histogram4[r8]       ; Load 4th histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 6                              ; Divide by 64 to get loop
                                                    ; iterations.


        vmovntdqa       zmm28, zmmword ptr [AllOnes]
        vmovntdqa       zmm29, zmmword ptr [AllNegativeOnes]
        vmovntdqa       zmm30, zmmword ptr [AllBinsMinusOne]
        vmovntdqa       zmm31, zmmword ptr [AllThirtyOne]

        mov             rax, 1111111111111111h
        kmovq           k1, rax
        vpmovm2b        zmm1, k1

        kshiftlq        k2, k1, 1
        vpmovm2b        zmm2, k2

        kshiftlq        k3, k1, 2
        vpmovm2b        zmm3, k3

        kshiftlq        k4, k1, 3
        vpmovm2b        zmm4, k4

        align 16

Chb30:  vmovntdqa       zmm0, zmmword ptr [rdx]     ; Load 64 bytes into zmm0.

;
; Advance the source input pointer by 64 bytes.  Clear the k1 mask
; which we'll use later for the vpgatherds.
;

        add             rdx, 40h                    ; Advance 64 bytes.

;
; Extract byte values for each doubleword into separate registers.
;

        ;IACA_VC_START

        vpandd          zmm5, zmm1, zmm0
        vpandd          zmm6, zmm2, zmm0
        vpandd          zmm7, zmm3, zmm0
        vpandd          zmm8, zmm4, zmm0

;
; Toggle all bits in the writemasks.
;

        kxnorq          k1, k5, k5
        kxnorq          k2, k6, k6
        kxnorq          k3, k7, k7
        kxnorq          k4, k0, k0

;
; Shift the second to fourth registers such that the byte value is moved to the
; least significant portion of the doubleword element of the register.
;

        vpsrldq         zmm6, zmm6, 1
        vpsrldq         zmm7, zmm7, 2
        vpsrldq         zmm8, zmm8, 3

;
; Gather the counts for the byte locations.
;

        vpgatherdd      zmm20 {k1}, [r8+4*zmm5]     ; Gather 1st counts.
        vpgatherdd      zmm21 {k2}, [r9+4*zmm6]     ; Gather 2nd counts.
        vpgatherdd      zmm22 {k3}, [r10+4*zmm7]    ; Gather 3rd counts.
        vpgatherdd      zmm23 {k4}, [r11+4*zmm8]    ; Gather 4th counts.

;
; Determine if the characters loaded into each ZMM register conflict.
;

        vpconflictd     zmm10, zmm5
        vpconflictd     zmm11, zmm6
        vpconflictd     zmm12, zmm7
        vpconflictd     zmm13, zmm8

        vpord           zmm14, zmm10, zmm11
        vpord           zmm15, zmm12, zmm13
        vpord           zmm14, zmm14, zmm15

        vptestmd        k1, zmm14, zmm14            ; Any conflicts anywhere?
        kortestw        k1, k1
        jne             Chb40                       ; At least one conflict.

;
; No conflicts across all registers.  Proceed with adding 1 (zmm28) to all
; the counts and then scattering the results back into memory.
;

        align           16

Chb35:  vpaddd          zmm24, zmm20, zmm28         ; Add 1st counts.
        vpaddd          zmm25, zmm21, zmm28         ; Add 2nd counts.
        vpaddd          zmm26, zmm22, zmm28         ; Add 3rd counts.
        vpaddd          zmm27, zmm23, zmm28         ; Add 4th counts.

;
; Toggle all bits in the writemasks.
;

        kxnorq          k1, k5, k5
        kxnorq          k2, k6, k6
        kxnorq          k3, k7, k7
        kxnorq          k4, k0, k0

;
; Scatter the counts back into their respective locations.
;

        vpscatterdd     [r8+4*zmm5]  {k1}, zmm24    ; Save 1st counts.
        vpscatterdd     [r9+4*zmm6]  {k2}, zmm25    ; Save 2nd counts.
        vpscatterdd     [r10+4*zmm7] {k3}, zmm26    ; Save 3rd counts.
        vpscatterdd     [r11+4*zmm8] {k4}, zmm27    ; Save 4th counts.

;
; Decrement loop counter and jump back to the top of the processing loop if
; there are more elements.
;

Chb38:  sub             ecx, 1                          ; Decrement counter.
        jnz             Chb30                           ; Continue if != 0.

;
; We've finished processing the input string, jump to finalization logic.
;

        jmp             Chb85

;
; Conflict handling logic.  At least one of the zmm registers had a conflict.
;


;
; Test the first register (zmm5) to see if it has any conflicts (zmm10).
;

Chb40:  vptestmd        k1, zmm10, zmm10            ; Test 1st for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm20         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chb47                 ; No conflicts.

;
; First register (zmm5) has a conflict (zmm10).
;

        align           16
Chb45:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm10       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chb45                 ; Continue loop if bits left

;
; Conflicts have been resolved for first register (zmm5), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chb47:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r8+4*zmm5] {k7}, zmm16     ; Save counts.

;
; Intentional follow-on.
;

;
; Test the second register (zmm6) to see if it has any conflicts (zmm11).
;

Chb50:  vptestmd        k1, zmm11, zmm11            ; Test 2nd for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm21         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chb57                 ; No conflicts.

;
; Second register (zmm6) has a conflict (zmm11).
;

        align           16
Chb55:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm11       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chb55                 ; Continue loop if bits left

;
; Conflicts have been resolved for second register (zmm6), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chb57:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r9+4*zmm6] {k7}, zmm16     ; Save counts.

;
; Intentional follow-on.
;

;
; Test the third register (zmm7) to see if it has any conflicts (zmm16).
;

Chb60:  vptestmd        k1, zmm12, zmm12            ; Test 3rd for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm22         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chb67                 ; No conflicts.

;
; Third register (zmm7) has a conflict (zmm16).
;

        align           16
Chb65:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm12       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chb65                 ; Continue loop if bits left

;
; Conflicts have been resolved for third register (zmm7), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chb67:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r10+4*zmm7] {k7}, zmm16    ; Save counts.

;
; Intentional follow-on.
;

;
; Test the second register (zmm8) to see if it has any conflicts (zmm13).
;

Chb70:  vptestmd        k1, zmm13, zmm13            ; Test 4th for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm23         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chb77                 ; No conflicts.

;
; Fourth register (zmm8) has a conflict (zmm13).
;

        align           16
Chb75:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm13       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chb75                 ; Continue loop if bits left

;
; Conflicts have been resolved for second register (zmm8), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chb77:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r11+4*zmm8] {k7}, zmm16    ; Save counts.

;
; Check for loop termination.
;

        sub             ecx, 1                          ; Decrement counter.
        jnz             Chb30                           ; Continue if != 0.

;
; Merge four histograms into one, 128 bytes at a time (two zmm registers in
; flight per histogram).
;

Chb85:  mov         ecx, 8
        xor         rax, rax

        align       16

Chb90:  vmovdqa32       zmm20, zmmword ptr [r8+rax]     ; Load 1st histo  0-63.
        vmovdqa32       zmm21, zmmword ptr [r8+rax+40h] ; Load 1st histo 64-127.

        vmovdqa32       zmm22, zmmword ptr [r9+rax]     ; Load 2nd histo  0-63.
        vmovdqa32       zmm23, zmmword ptr [r9+rax+40h] ; Load 2nd histo 64-127.

        vmovdqa32       zmm24, zmmword ptr [r10+rax]    ; Load 3rd histo  0-63.
        vmovdqa32       zmm25, zmmword ptr [r10+rax+40h]; Load 3rd histo 64-127.

        vmovdqa32       zmm26, zmmword ptr [r11+rax]    ; Load 4th histo  0-63.
        vmovdqa32       zmm27, zmmword ptr [r11+rax+40h]; Load 4th histo 64-127.

        vpaddd          zmm10, zmm20, zmm22             ; Add 1-2 0-63 histo.
        vpaddd          zmm11, zmm24, zmm26             ; Add 3-4 0-63 histo.
        vpaddd          zmm12, zmm10, zmm11             ; Merge 0-63 histo.

        vpaddd          zmm13, zmm21, zmm23             ; Add 1-2 64-127 histo.
        vpaddd          zmm14, zmm25, zmm27             ; Add 3-4 64-127 histo.
        vpaddd          zmm15, zmm13, zmm14             ; Merge 64-127 histo.

        vmovdqa32       zmmword ptr [r8+rax], zmm12     ; Save  0-63  histo.
        vmovdqa32       zmmword ptr [r8+rax+40h], zmm15 ; Save 64-127 histo.

        add             rax, 80h                        ; Advance to next 128b.
        sub             ecx, 1                          ; Decrement loop cntr.
        jnz             Chb90                           ; Continue if != 0.

;
; Indicate success.
;

        mov     rax, 1

;
; Restore non-volatile registers.
;

Chb99:
        mov             rbp,   Locals.SavedRbp[rsp]
        mov             rbx,   Locals.SavedRbx[rsp]
        mov             rdi,   Locals.SavedRdi[rsp]
        mov             rsi,   Locals.SavedRsi[rsp]
        mov             r12,   Locals.SavedR12[rsp]
        mov             r13,   Locals.SavedR13[rsp]
        mov             r14,   Locals.SavedR14[rsp]
        mov             r15,   Locals.SavedR15[rsp]

        movdqa          xmm6,  Locals.SavedXmm6[rsp]
        movdqa          xmm7,  Locals.SavedXmm7[rsp]
        movdqa          xmm8,  Locals.SavedXmm8[rsp]
        movdqa          xmm9,  Locals.SavedXmm9[rsp]
        movdqa          xmm10, Locals.SavedXmm10[rsp]
        movdqa          xmm11, Locals.SavedXmm11[rsp]
        movdqa          xmm12, Locals.SavedXmm12[rsp]
        movdqa          xmm13, Locals.SavedXmm13[rsp]
        movdqa          xmm14, Locals.SavedXmm14[rsp]
        movdqa          xmm15, Locals.SavedXmm15[rsp]

;
; Begin epilogue.  Deallocate stack space and return.
;

        add     rsp, LOCALS_SIZE
        ret

        ;IACA_VC_END

        NESTED_END CreateHistogramAvx512AlignedAsm, _TEXT$00

;
; Tweak ordering to improve IACA static analysis.
;

        NESTED_ENTRY CreateHistogramAvx512AlignedAsm_v2, _TEXT$00

;
; Begin prologue.  Allocate stack space and save non-volatile registers.
;

        alloc_stack LOCALS_SIZE

        save_reg    rbp, Locals.SavedRbp        ; Save non-volatile rbp.
        save_reg    rbx, Locals.SavedRbx        ; Save non-volatile rbx.
        save_reg    rdi, Locals.SavedRdi        ; Save non-volatile rdi.
        save_reg    rsi, Locals.SavedRsi        ; Save non-volatile rsi.
        save_reg    r12, Locals.SavedR12        ; Save non-volatile r12.
        save_reg    r13, Locals.SavedR13        ; Save non-volatile r13.
        save_reg    r14, Locals.SavedR14        ; Save non-volatile r14.
        save_reg    r15, Locals.SavedR15        ; Save non-volatile r15.

        save_xmm128 xmm6, Locals.SavedXmm6      ; Save non-volatile xmm6.
        save_xmm128 xmm7, Locals.SavedXmm7      ; Save non-volatile xmm7.
        save_xmm128 xmm8, Locals.SavedXmm8      ; Save non-volatile xmm8.
        save_xmm128 xmm9, Locals.SavedXmm9      ; Save non-volatile xmm9.
        save_xmm128 xmm10, Locals.SavedXmm10    ; Save non-volatile xmm10.
        save_xmm128 xmm11, Locals.SavedXmm11    ; Save non-volatile xmm11.
        save_xmm128 xmm12, Locals.SavedXmm12    ; Save non-volatile xmm12.
        save_xmm128 xmm13, Locals.SavedXmm13    ; Save non-volatile xmm13.
        save_xmm128 xmm14, Locals.SavedXmm14    ; Save non-volatile xmm14.
        save_xmm128 xmm15, Locals.SavedXmm15    ; Save non-volatile xmm15.

        END_PROLOGUE


;
; Clear return value (Success = FALSE).
;

        ;IACA_VC_START

        xor     rax, rax                                ; Clear rax.

;
; Validate parameters.
;

        test    rcx, rcx                                ; Is rcx NULL?
        jz      Chc99                                   ; Yes, abort.
        test    rdx, rdx                                ; Is rdx NULL?
        jz      Chc99                                   ; Yes, abort.

;
; Verify the string is at least 64 bytes long.
;
        mov     r9, 64                                  ; Initialize r9 to 64.
        cmp     String.Length[rcx], r9d                 ; Compare Length to 64.
        jl      Chc99                                   ; String is too short.

;
; Ensure the incoming string and histogram buffers are aligned to 32-byte
; boundaries.
;

        mov     r9, 31                                  ; Initialize r9 to 31.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Chc99                                   ; No, abort.

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
;   r10 - Base address of third histogram buffer.
;
;   r11 - Base address of fourth histogram buffer.
;

        mov     r8,  rdx                            ; Load 1st histo buffer.
        lea     r9,  Histogram.Histogram2[r8]       ; Load 2nd histo buffer.
        lea     r10, Histogram.Histogram3[r8]       ; Load 3rd histo buffer.
        lea     r11, Histogram.Histogram4[r8]       ; Load 4th histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 6                              ; Divide by 64 to get loop
                                                    ; iterations.


        vmovntdqa       zmm28, zmmword ptr [AllOnes]
        vmovntdqa       zmm29, zmmword ptr [AllNegativeOnes]
        vmovntdqa       zmm30, zmmword ptr [AllBinsMinusOne]
        vmovntdqa       zmm31, zmmword ptr [AllThirtyOne]

        mov             rax, 1111111111111111h
        kmovq           k1, rax
        vpmovm2b        zmm1, k1

        kshiftlq        k2, k1, 1
        vpmovm2b        zmm2, k2

        kshiftlq        k3, k1, 2
        vpmovm2b        zmm3, k3

        kshiftlq        k4, k1, 3
        vpmovm2b        zmm4, k4

        align 16

Chc30:  vmovntdqa       zmm0, zmmword ptr [rdx]     ; Load 64 bytes into zmm0.

;
; Advance the source input pointer by 64 bytes.  Clear the k1 mask
; which we'll use later for the vpgatherds.
;

        add             rdx, 40h                    ; Advance 64 bytes.

;
; Extract byte values for each doubleword into separate registers.
;

        ;IACA_VC_START

        kxnorq          k1, k5, k5
        vpandd          zmm5, zmm1, zmm0
        vpgatherdd      zmm20 {k1}, [r8+4*zmm5]     ; Gather 1st counts.
        vpconflictd     zmm10, zmm5

        kxnorq          k2, k6, k6
        vpandd          zmm6, zmm2, zmm0
        vpsrldq         zmm6, zmm6, 1
        vpgatherdd      zmm21 {k2}, [r9+4*zmm6]     ; Gather 2nd counts.
        vpconflictd     zmm11, zmm6

        vpord           zmm14, zmm10, zmm11

        kxnorq          k3, k7, k7
        vpandd          zmm7, zmm3, zmm0
        vpsrldq         zmm7, zmm7, 2
        vpgatherdd      zmm22 {k3}, [r10+4*zmm7]    ; Gather 3rd counts.
        vpconflictd     zmm12, zmm7

        vpandd          zmm8, zmm4, zmm0
        vpsrldq         zmm8, zmm8, 3
        kxnorq          k4, k0, k0
        vpgatherdd      zmm23 {k4}, [r11+4*zmm8]    ; Gather 4th counts.
        vpconflictd     zmm13, zmm8

        vpord           zmm15, zmm12, zmm13
        vpord           zmm14, zmm14, zmm15

        vptestmd        k1, zmm14, zmm14            ; Any conflicts anywhere?
        kortestw        k1, k1
        jne             Chc40                       ; At least one conflict.

;
; No conflicts across all registers.  Proceed with adding 1 (zmm28) to all
; the counts and then scattering the results back into memory.
;

        align           16

Chc35:  kxnorq          k1, k5, k5
        vpaddd          zmm24, zmm20, zmm28         ; Add 1st counts.
        vpscatterdd     [r8+4*zmm5]  {k1}, zmm24    ; Save 1st counts.

        kxnorq          k2, k6, k6
        vpaddd          zmm25, zmm21, zmm28         ; Add 2nd counts.
        vpscatterdd     [r9+4*zmm6]  {k2}, zmm25    ; Save 2nd counts.

        kxnorq          k3, k7, k7
        vpaddd          zmm26, zmm22, zmm28         ; Add 3rd counts.
        vpscatterdd     [r10+4*zmm7] {k3}, zmm26    ; Save 3rd counts.

        kxnorq          k4, k0, k0
        vpaddd          zmm27, zmm23, zmm28         ; Add 4th counts.
        vpscatterdd     [r11+4*zmm8] {k4}, zmm27    ; Save 4th counts.


Chc38:  sub             ecx, 1                          ; Decrement counter.
        jnz             Chc30                           ; Continue if != 0.

;
; We've finished processing the input string, jump to finalization logic.
;

        jmp             Chc85

;
; Conflict handling logic.  At least one of the zmm registers had a conflict.
;


;
; Test the first register (zmm5) to see if it has any conflicts (zmm10).
;

Chc40:  vptestmd        k1, zmm10, zmm10            ; Test 1st for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm20         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chc47                 ; No conflicts.

;
; First register (zmm5) has a conflict (zmm10).
;

        align           16
Chc45:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm10       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chc45                 ; Continue loop if bits left

;
; Conflicts have been resolved for first register (zmm5), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc47:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r8+4*zmm5] {k7}, zmm16     ; Save counts.

;
; Intentional follow-on.
;

;
; Test the second register (zmm6) to see if it has any conflicts (zmm11).
;

Chc50:  vptestmd        k1, zmm11, zmm11            ; Test 2nd for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm21         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chc57                 ; No conflicts.

;
; Second register (zmm6) has a conflict (zmm11).
;

        align           16
Chc55:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm11       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chc55                 ; Continue loop if bits left

;
; Conflicts have been resolved for second register (zmm6), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc57:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r9+4*zmm6] {k7}, zmm16     ; Save counts.

;
; Intentional follow-on.
;

;
; Test the third register (zmm7) to see if it has any conflicts (zmm16).
;

Chc60:  vptestmd        k1, zmm12, zmm12            ; Test 3rd for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm22         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chc67                 ; No conflicts.

;
; Third register (zmm7) has a conflict (zmm16).
;

        align           16
Chc65:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm12       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chc65                 ; Continue loop if bits left

;
; Conflicts have been resolved for third register (zmm7), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc67:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r10+4*zmm7] {k7}, zmm16    ; Save counts.

;
; Intentional follow-on.
;

;
; Test the second register (zmm8) to see if it has any conflicts (zmm13).
;

Chc70:  vptestmd        k1, zmm13, zmm13            ; Test 4th for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kmovw           ebx, k1                     ; Move mask into ebx.
        vpaddd          zmm16, zmm16, zmm23         ; Add partial counts.
        test            ebx, ebx                    ; Any conflicts?
        jz              short Chc77                 ; No conflicts.

;
; Fourth register (zmm8) has a conflict (zmm13).
;

        align           16
Chc75:  vpbroadcastd    zmm18, ebx                  ; Bcast mask into zmm18.
        kmovw           k1, ebx                     ; Move mask into k1.
        vpaddd          zmm16 {k1}, zmm16, zmm28    ; Add masked counts.
        vptestmd        k0 {k1}, zmm18, zmm13       ; Test against conflict.
        kmovw           esi, k0                     ; Move new mask into esi.
        and             ebx, esi                    ; Mask off recent bit.
        jnz             short Chc75                 ; Continue loop if bits left

;
; Conflicts have been resolved for second register (zmm8), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc77:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpscatterdd     [r11+4*zmm8] {k7}, zmm16    ; Save counts.

;
; Check for loop termination.
;

        sub             ecx, 1                          ; Decrement counter.
        jnz             Chc30                           ; Continue if != 0.

;
; Merge four histograms into one, 128 bytes at a time (two zmm registers in
; flight per histogram).
;

Chc85:  mov         ecx, 8
        xor         rax, rax

        align       16

Chc90:  vmovdqa32       zmm20, zmmword ptr [r8+rax]     ; Load 1st histo  0-63.
        vmovdqa32       zmm22, zmmword ptr [r9+rax]     ; Load 2nd histo  0-63.
        vpaddd          zmm10, zmm20, zmm22             ; Add 1-2 0-63 histo.

        vmovdqa32       zmm21, zmmword ptr [r8+rax+40h] ; Load 1st histo 64-127.
        vmovdqa32       zmm23, zmmword ptr [r9+rax+40h] ; Load 2nd histo 64-127.
        vpaddd          zmm13, zmm21, zmm23             ; Add 1-2 64-127 histo.

        vmovdqa32       zmm24, zmmword ptr [r10+rax]    ; Load 3rd histo  0-63.
        vmovdqa32       zmm26, zmmword ptr [r11+rax]    ; Load 4th histo  0-63.
        vpaddd          zmm11, zmm24, zmm26             ; Add 3-4 0-63 histo.

        vmovdqa32       zmm25, zmmword ptr [r10+rax+40h]; Load 3rd histo 64-127.
        vmovdqa32       zmm27, zmmword ptr [r11+rax+40h]; Load 4th histo 64-127.
        vpaddd          zmm14, zmm25, zmm27             ; Add 3-4 64-127 histo.

        vpaddd          zmm12, zmm10, zmm11             ; Merge 0-63 histo.
        vmovdqa32       zmmword ptr [r8+rax], zmm12     ; Save  0-63  histo.

        vpaddd          zmm15, zmm13, zmm14             ; Merge 64-127 histo.
        vmovdqa32       zmmword ptr [r8+rax+40h], zmm15 ; Save 64-127 histo.

        add             rax, 80h                        ; Advance to next 128b.
        sub             ecx, 1                          ; Decrement loop cntr.
        jnz             Chc90                           ; Continue if != 0.

;
; Indicate success.
;

        mov     rax, 1

;
; Restore non-volatile registers.
;

Chc99:
        mov             rbp,   Locals.SavedRbp[rsp]
        mov             rbx,   Locals.SavedRbx[rsp]
        mov             rdi,   Locals.SavedRdi[rsp]
        mov             rsi,   Locals.SavedRsi[rsp]
        mov             r12,   Locals.SavedR12[rsp]
        mov             r13,   Locals.SavedR13[rsp]
        mov             r14,   Locals.SavedR14[rsp]
        mov             r15,   Locals.SavedR15[rsp]

        movdqa          xmm6,  Locals.SavedXmm6[rsp]
        movdqa          xmm7,  Locals.SavedXmm7[rsp]
        movdqa          xmm8,  Locals.SavedXmm8[rsp]
        movdqa          xmm9,  Locals.SavedXmm9[rsp]
        movdqa          xmm10, Locals.SavedXmm10[rsp]
        movdqa          xmm11, Locals.SavedXmm11[rsp]
        movdqa          xmm12, Locals.SavedXmm12[rsp]
        movdqa          xmm13, Locals.SavedXmm13[rsp]
        movdqa          xmm14, Locals.SavedXmm14[rsp]
        movdqa          xmm15, Locals.SavedXmm15[rsp]

;
; Begin epilogue.  Deallocate stack space and return.
;

        add     rsp, LOCALS_SIZE
        ret

        ;IACA_VC_END

        NESTED_END CreateHistogramAvx512AlignedAsm_v2, _TEXT$00

        NESTED_ENTRY CreateHistogramAvx512AlignedAsm_v3, _TEXT$00

;
; Begin prologue.  Allocate stack space and save non-volatile registers.
;

        alloc_stack LOCALS_SIZE

        save_reg    rbp, Locals.SavedRbp        ; Save non-volatile rbp.
        save_reg    rbx, Locals.SavedRbx        ; Save non-volatile rbx.
        save_reg    rdi, Locals.SavedRdi        ; Save non-volatile rdi.
        save_reg    rsi, Locals.SavedRsi        ; Save non-volatile rsi.
        save_reg    r12, Locals.SavedR12        ; Save non-volatile r12.
        save_reg    r13, Locals.SavedR13        ; Save non-volatile r13.
        save_reg    r14, Locals.SavedR14        ; Save non-volatile r14.
        save_reg    r15, Locals.SavedR15        ; Save non-volatile r15.

        save_xmm128 xmm6, Locals.SavedXmm6      ; Save non-volatile xmm6.
        save_xmm128 xmm7, Locals.SavedXmm7      ; Save non-volatile xmm7.
        save_xmm128 xmm8, Locals.SavedXmm8      ; Save non-volatile xmm8.
        save_xmm128 xmm9, Locals.SavedXmm9      ; Save non-volatile xmm9.
        save_xmm128 xmm10, Locals.SavedXmm10    ; Save non-volatile xmm10.
        save_xmm128 xmm11, Locals.SavedXmm11    ; Save non-volatile xmm11.
        save_xmm128 xmm12, Locals.SavedXmm12    ; Save non-volatile xmm12.
        save_xmm128 xmm13, Locals.SavedXmm13    ; Save non-volatile xmm13.
        save_xmm128 xmm14, Locals.SavedXmm14    ; Save non-volatile xmm14.
        save_xmm128 xmm15, Locals.SavedXmm15    ; Save non-volatile xmm15.

        END_PROLOGUE


;
; Clear return value (Success = FALSE).
;

        ;IACA_VC_START

        xor     rax, rax                                ; Clear rax.

;
; Validate parameters.
;

        test    rcx, rcx                                ; Is rcx NULL?
        jz      Chc99                                   ; Yes, abort.
        test    rdx, rdx                                ; Is rdx NULL?
        jz      Chc99                                   ; Yes, abort.

;
; Verify the string is at least 64 bytes long.
;
        mov     r9, 64                                  ; Initialize r9 to 64.
        cmp     String.Length[rcx], r9d                 ; Compare Length to 64.
        jl      Chc99                                   ; String is too short.

;
; Ensure the incoming string and histogram buffers are aligned to 32-byte
; boundaries.
;

        mov     r9, 31                                  ; Initialize r9 to 31.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Chc99                                   ; No, abort.

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
;   r10 - Base address of third histogram buffer.
;
;   r11 - Base address of fourth histogram buffer.
;

        mov     r8,  rdx                            ; Load 1st histo buffer.
        lea     r9,  Histogram.Histogram2[r8]       ; Load 2nd histo buffer.
        lea     r10, Histogram.Histogram3[r8]       ; Load 3rd histo buffer.
        lea     r11, Histogram.Histogram4[r8]       ; Load 4th histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 6                              ; Divide by 64 to get loop
                                                    ; iterations.


        vmovntdqa       zmm28, zmmword ptr [AllOnes]
        vmovntdqa       zmm29, zmmword ptr [AllNegativeOnes]
        vmovntdqa       zmm30, zmmword ptr [AllBinsMinusOne]
        vmovntdqa       zmm31, zmmword ptr [AllThirtyOne]

        mov             rax, 1111111111111111h
        kmovq           k1, rax
        vpmovm2b        zmm1, k1

        kshiftlq        k2, k1, 1
        vpmovm2b        zmm2, k2

        kshiftlq        k3, k1, 2
        vpmovm2b        zmm3, k3

        kshiftlq        k4, k1, 3
        vpmovm2b        zmm4, k4

        align 16

Chc30:  vmovntdqa       zmm0, zmmword ptr [rdx]     ; Load 64 bytes into zmm0.

;
; Advance the source input pointer by 64 bytes.  Clear the k1 mask
; which we'll use later for the vpgatherds.
;

        add             rdx, 40h                    ; Advance 64 bytes.

;
; Extract byte values for each doubleword into separate registers.
;

        ;IACA_VC_START

        kxnorq          k1, k5, k5
        vpandd          zmm5, zmm1, zmm0
        vpgatherdd      zmm20 {k1}, [r8+4*zmm5]     ; Gather 1st counts.
        vpconflictd     zmm10, zmm5

        kxnorq          k2, k6, k6
        vpandd          zmm6, zmm2, zmm0
        vpsrldq         zmm6, zmm6, 1
        vpgatherdd      zmm21 {k2}, [r9+4*zmm6]     ; Gather 2nd counts.
        vpconflictd     zmm11, zmm6

        vpord           zmm14, zmm10, zmm11

        kxnorq          k3, k7, k7
        vpandd          zmm7, zmm3, zmm0
        vpsrldq         zmm7, zmm7, 2
        vpgatherdd      zmm22 {k3}, [r10+4*zmm7]    ; Gather 3rd counts.
        vpconflictd     zmm12, zmm7

        vpandd          zmm8, zmm4, zmm0
        vpsrldq         zmm8, zmm8, 3
        kxnorq          k4, k0, k0
        vpgatherdd      zmm23 {k4}, [r11+4*zmm8]    ; Gather 4th counts.
        vpconflictd     zmm13, zmm8

        vpord           zmm15, zmm12, zmm13
        vpord           zmm14, zmm14, zmm15

        vptestmd        k1, zmm14, zmm14            ; Any conflicts anywhere?
        kortestw        k1, k1
        jne             Chc40                       ; At least one conflict.

;
; No conflicts across all registers.  Proceed with adding 1 (zmm28) to all
; the counts and then scattering the results back into memory.
;

        align           16

Chc35:  kxnorq          k1, k5, k5
        vpaddd          zmm24, zmm20, zmm28         ; Add 1st counts.
        vpscatterdd     [r8+4*zmm5]  {k1}, zmm24    ; Save 1st counts.

        kxnorq          k2, k6, k6
        vpaddd          zmm25, zmm21, zmm28         ; Add 2nd counts.
        vpscatterdd     [r9+4*zmm6]  {k2}, zmm25    ; Save 2nd counts.

        kxnorq          k3, k7, k7
        vpaddd          zmm26, zmm22, zmm28         ; Add 3rd counts.
        vpscatterdd     [r10+4*zmm7] {k3}, zmm26    ; Save 3rd counts.

        kxnorq          k4, k0, k0
        vpaddd          zmm27, zmm23, zmm28         ; Add 4th counts.
        vpscatterdd     [r11+4*zmm8] {k4}, zmm27    ; Save 4th counts.


Chc38:  sub             ecx, 1                          ; Decrement counter.
        jnz             Chc30                           ; Continue if != 0.

;
; We've finished processing the input string, jump to finalization logic.
;

        jmp             Chc85

;
; Conflict handling logic.  At least one of the zmm registers had a conflict.
;


;
; Test the first register (zmm5) to see if it has any conflicts (zmm10).
;

Chc40:  vptestmd        k1, zmm10, zmm10            ; Test 1st for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kortestw        k1, k1                      ; Any conflicts?
        jz              short Chc47                 ; No conflicts.

;
; First register (zmm5) has a conflict (zmm10).
;

        vplzcntd        zmm10, zmm10                ; Count leading zeros.
        vpsubd          zmm10, zmm31, zmm10         ; Subtract 31 from each cnt.

        align           16

Chc45:  vpermd          zmm18 {k1} {z}, zmm10, zmm16
        vpermd          zmm10 {k1},     zmm10, zmm10
        vpaddd          zmm16 {k1},     zmm16, zmm18
        vpcmpd          k1, zmm29, zmm10, OP_NEQ    ; Generate new mask.
        kortestw        k1, k1
        jne             short Chc45

;
; Conflicts have been resolved for first register (zmm5), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc47:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpaddd          zmm16, zmm16, zmm20         ; Add partial counts.
        vpscatterdd     [r8+4*zmm5] {k7}, zmm16     ; Save counts.

;
; Intentional follow-on.
;

;
; Test the second register (zmm6) to see if it has any conflicts (zmm11).
;

Chc50:  vptestmd        k1, zmm11, zmm11            ; Test 2nd for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kortestw        k1, k1                      ; Any conflicts?
        jz              short Chc57                 ; No conflicts.

;
; Second register (zmm6) has a conflict (zmm11).
;

        vplzcntd        zmm10, zmm11                ; Count leading zeros.
        vpsubd          zmm10, zmm31, zmm10         ; Subtract 31 from each cnt.

        align           16

Chc55:  vpermd          zmm18 {k1} {z}, zmm10, zmm16
        vpermd          zmm10 {k1},     zmm10, zmm10
        vpaddd          zmm16 {k1},     zmm16, zmm18
        vpcmpd          k1, zmm29, zmm10, OP_NEQ    ; Generate new mask.
        kortestw        k1, k1
        jne             short Chc55

;
; Conflicts have been resolved for second register (zmm6), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc57:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpaddd          zmm16, zmm16, zmm21         ; Add partial counts.
        vpscatterdd     [r9+4*zmm6] {k7}, zmm16     ; Save counts.

;
; Intentional follow-on.
;

;
; Test the third register (zmm7) to see if it has any conflicts (zmm12).
;

Chc60:  vptestmd        k1, zmm12, zmm12            ; Test 3rd for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kortestw        k1, k1                      ; Any conflicts?
        jz              short Chc67                 ; No conflicts.


;
; Third register (zmm7) has a conflict (zmm12).
;

        vplzcntd        zmm10, zmm12                ; Count leading zeros.
        vpsubd          zmm10, zmm31, zmm10         ; Subtract 31 from each cnt.

        align           16

Chc65:  vpermd          zmm18 {k1} {z}, zmm10, zmm16
        vpermd          zmm10 {k1},     zmm10, zmm10
        vpaddd          zmm16 {k1},     zmm16, zmm18
        vpcmpd          k1, zmm29, zmm10, OP_NEQ    ; Generate new mask.
        kortestw        k1, k1
        jne             short Chc65

;
; Conflicts have been resolved for third register (zmm7), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc67:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpaddd          zmm16, zmm16, zmm22         ; Add partial counts.
        vpscatterdd     [r10+4*zmm7] {k7}, zmm16    ; Save counts.

;
; Intentional follow-on.
;

;
; Test the fourth register (zmm8) to see if it has any conflicts (zmm13).
;

Chc70:  vptestmd        k1, zmm13, zmm13            ; Test 4th for conflicts.
        vmovaps         zmm16, zmm28                ; Copy AllOnes into zmm16.
        kortestw        k1, k1                      ; Any conflicts?
        jz              short Chc77                 ; No conflicts.

;
; Fourth register (zmm8) has a conflict (zmm13).
;

        vplzcntd        zmm10, zmm13                ; Count leading zeros.
        vpsubd          zmm10, zmm31, zmm10         ; Subtract 31 from each cnt.

        align           16

Chc75:  vpermd          zmm18 {k1} {z}, zmm10, zmm16
        vpermd          zmm10 {k1},     zmm10, zmm10
        vpaddd          zmm16 {k1},     zmm16, zmm18
        vpcmpd          k1, zmm29, zmm10, OP_NEQ    ; Generate new mask.
        kortestw        k1, k1
        jne             short Chc75

;
; Conflicts have been resolved for second register (zmm8), with the final count
; now living in zmm16.  Scatter back to memory.
;

Chc77:  kxnorw          k7, k3, k3                  ; Set all bits in writemask.
        vpaddd          zmm16, zmm16, zmm23         ; Add partial counts.
        vpscatterdd     [r11+4*zmm8] {k7}, zmm16    ; Save counts.

;
; Check for loop termination.
;

        sub             ecx, 1                          ; Decrement counter.
        jnz             Chc30                           ; Continue if != 0.

;
; Merge four histograms into one, 128 bytes at a time (two zmm registers in
; flight per histogram).
;

Chc85:  mov             ecx, 8
        xor             rax, rax

        align           16

Chc90:  vmovdqa32       zmm20, zmmword ptr [r8+rax]     ; Load 1st histo  0-63.
        vmovdqa32       zmm22, zmmword ptr [r9+rax]     ; Load 2nd histo  0-63.
        vpaddd          zmm10, zmm20, zmm22             ; Add 1-2 0-63 histo.

        vmovdqa32       zmm21, zmmword ptr [r8+rax+40h] ; Load 1st histo 64-127.
        vmovdqa32       zmm23, zmmword ptr [r9+rax+40h] ; Load 2nd histo 64-127.
        vpaddd          zmm13, zmm21, zmm23             ; Add 1-2 64-127 histo.

        vmovdqa32       zmm24, zmmword ptr [r10+rax]    ; Load 3rd histo  0-63.
        vmovdqa32       zmm26, zmmword ptr [r11+rax]    ; Load 4th histo  0-63.
        vpaddd          zmm11, zmm24, zmm26             ; Add 3-4 0-63 histo.

        vmovdqa32       zmm25, zmmword ptr [r10+rax+40h]; Load 3rd histo 64-127.
        vmovdqa32       zmm27, zmmword ptr [r11+rax+40h]; Load 4th histo 64-127.
        vpaddd          zmm14, zmm25, zmm27             ; Add 3-4 64-127 histo.

        vpaddd          zmm12, zmm10, zmm11             ; Merge 0-63 histo.
        vmovdqa32       zmmword ptr [r8+rax], zmm12     ; Save  0-63  histo.

        vpaddd          zmm15, zmm13, zmm14             ; Merge 64-127 histo.
        vmovdqa32       zmmword ptr [r8+rax+40h], zmm15 ; Save 64-127 histo.

        add             rax, 80h                        ; Advance to next 128b.
        sub             ecx, 1                          ; Decrement loop cntr.
        jnz             Chc90                           ; Continue if != 0.


;
; Shift rax (0x400) right 10 to indicate 1, our success return value.
;
        shr             rax, 10

;
; Restore non-volatile registers.
;

Chc99:
        mov             rbp,   Locals.SavedRbp[rsp]
        mov             rbx,   Locals.SavedRbx[rsp]
        mov             rdi,   Locals.SavedRdi[rsp]
        mov             rsi,   Locals.SavedRsi[rsp]
        mov             r12,   Locals.SavedR12[rsp]
        mov             r13,   Locals.SavedR13[rsp]
        mov             r14,   Locals.SavedR14[rsp]
        mov             r15,   Locals.SavedR15[rsp]

        movdqa          xmm6,  Locals.SavedXmm6[rsp]
        movdqa          xmm7,  Locals.SavedXmm7[rsp]
        movdqa          xmm8,  Locals.SavedXmm8[rsp]
        movdqa          xmm9,  Locals.SavedXmm9[rsp]
        movdqa          xmm10, Locals.SavedXmm10[rsp]
        movdqa          xmm11, Locals.SavedXmm11[rsp]
        movdqa          xmm12, Locals.SavedXmm12[rsp]
        movdqa          xmm13, Locals.SavedXmm13[rsp]
        movdqa          xmm14, Locals.SavedXmm14[rsp]
        movdqa          xmm15, Locals.SavedXmm15[rsp]

;
; Begin epilogue.  Deallocate stack space and return.
;

        add     rsp, LOCALS_SIZE
        ret

        ;IACA_VC_END

        NESTED_END CreateHistogramAvx512AlignedAsm_v3, _TEXT$00


        NESTED_ENTRY CreateHistogramAvx512AlignedAsm_v4, _TEXT$00

;
; Begin prologue.  Allocate stack space and save non-volatile registers.
;

        alloc_stack LOCALS_SIZE

        save_xmm128 xmm6, Locals.SavedXmm6      ; Save non-volatile xmm6.
        save_xmm128 xmm7, Locals.SavedXmm7      ; Save non-volatile xmm7.
        save_xmm128 xmm8, Locals.SavedXmm8      ; Save non-volatile xmm8.
        save_xmm128 xmm9, Locals.SavedXmm9      ; Save non-volatile xmm9.

        END_PROLOGUE


;
; Clear return value (Success = FALSE).
;

        IACA_VC_START

        xor     rax, rax                                ; Clear rax.

;
; Validate parameters.
;

        test    rcx, rcx                                ; Is rcx NULL?
        jz      Chf99                                   ; Yes, abort.
        test    rdx, rdx                                ; Is rdx NULL?
        jz      Chf99                                   ; Yes, abort.

;
; Verify the string is at least 64 bytes long.
;
        mov     r9, 64                                  ; Initialize r9 to 64.
        cmp     String.Length[rcx], r9d                 ; Compare Length to 64.
        jl      Chf99                                   ; String is too short.

;
; Ensure the incoming string and histogram buffers are aligned to 32-byte
; boundaries.
;

        mov     r9, 31                                  ; Initialize r9 to 31.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Chf99                                   ; No, abort.

;
; Initialize loop variables.
;
;   rax - Counter for which byte of a doubleword we're processing.
;
;   rcx - Counter (length of string in multiples of 64).
;
;   rdx - Base string buffer.
;
;   r8  - Base address of first histogram buffer.
;
;   r9  - Inverted version of rax.  E.g. if:
;           rax == 0, r9 == 4
;           rax == 1, r9 == 3
;           rax == 2, r9 == 2
;           rax == 3, r9 == 1
;           rax == 4, r9 == 0
;
;   r10 - Unused.
;
;   r11 - Unused.
;
;   zmm0 - Receives 64 bytes of the input string.
;
;   zmm1 - Vectorized mask isolating the byte we're interested in for a given
;          round.  E.g. 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff.
;
;          N.B. We start off with byte 4 and work backwards to the first byte.
;
;   zmm2 - Receives the isolated byte in each doubleword.
;
;   zmm3 - Receives the gathered counts for a given set of bytes being
;       processed.
;
;   zmm4 - Used as the permute control register.  Initially captures the result
;       of the conflict comparison, and then the leading zero count.
;
;   zmm5 - Used to capture partial counts when handling conflicts.
;
;   zmm6 - Used for permuting counts as per the control register.
;
;   zmm7 - Vectorized mask base used at the top of the loop.
;
;   zmm28 - AllOnes
;
;   zmm29 - AllNegativeOnes
;
;   zmm31 - AllThirtyOne
;

        mov     r8,  rdx                            ; Load 1st histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 6                              ; Divide by 64 to get loop
                                                    ; iterations.


        vmovntdqa       zmm28, zmmword ptr [AllOnes]
        vmovntdqa       zmm29, zmmword ptr [AllNegativeOnes]
        vmovntdqa       zmm31, zmmword ptr [AllThirtyOne]

;
; Isolate the 4th byte in every doubleword.
;

        xor             rax, rax
        not             al
        vpbroadcastd    zmm7, eax

        align 16

Chf15:  vmovntdqa       zmm0, zmmword ptr [rdx]     ; Load 64 bytes into zmm0.
        mov             eax, 4                      ; Initialize eax counter.
        xor             r9, r9                      ; Clear r9.
        add             rdx, 40h                    ; Advance buffer 64 bytes.
        vmovaps         zmm1, zmm7                  ; Initialize byte mask.
        vpxord          zmm9, zmm9, zmm9            ; Clear shift vector.

        align 16

Chf20:  kxnorw          k1, k5, k5
        vpandd          zmm2, zmm1, zmm0
        vpsrlq          zmm2, zmm2, xmm9

        vpxord          zmm3, zmm3, zmm3            ; Clear counts.
        vpgatherdd      zmm3 {k1}, [r8+4*zmm2]      ; Gather counts for bytes.
        vpconflictd     zmm4, zmm2                  ; Detect conflicts.
        vmovaps         zmm5, zmm28                 ; Copy AllOnes register.

;
; Increment the 0..3 counter (r9) by one, shift the vectorized mask control
; (zmm1) right by one quadword.
;

        add             r9, 8
        movd            xmm9, r9
        vpslldq         zmm1, zmm1, 1

;
; Were there any conflicts?
;

        vptestmd        k1, zmm4, zmm4              ; Generate conflict mask.
        kortestw        k1, k1                      ; Any conflicts?
        je              Chf27                       ; No conflicts, update.

;
; There was at least one conflict.  Proceed with the conflict handling logic.
;

        vplzcntd        zmm4, zmm4                  ; Count leading zeros.
        vpsubd          zmm4, zmm31, zmm4           ; Subtract 31 from elems.

Chf25:  vpermd          zmm6 {k1} {z}, zmm4, zmm5   ; Capture partials.
        vpermd          zmm4 {k1},     zmm4, zmm4   ; Permute control reg.
        vpaddd          zmm5 {k1},     zmm5, zmm6   ; Add counts.
        vpcmpd          k1, zmm4, zmm29, OP_NEQ     ; Generate new mask.
        kortestw        k1, k1                      ; Any more conflicts?
        jne             short Chf25                 ; Yes, jump back and repeat.

;
; If we get here, we've resolved all the conflicts (if there were any), and
; need to add the final counts together then scatter everything back to its
; appropriate location.
;

Chf27:  kxnorw          k1, k6, k6                  ; Set all bits in writemask.
        vpaddd          zmm3, zmm3, zmm5            ; Add counts to total.
        vpscatterdd     [r8+4*zmm2] {k1}, zmm3      ; Save counts.
        sub             rax, 1                      ; Decrement loop counter.
        jnz             Chf20                       ; If nz, jump back to cont.

;
; We've finished processing all four bytes in each doubleword in each vector.
; Check for loop termination against the rcx counter.
;

        sub             ecx, 1                      ; Decrement counter.
        jnz             Chf15                       ; Continue if != 0.

;
; No more bytes to process.  Indicate success and return.
;

        mov             rax, 1

;
; Restore non-volatile registers.
;

Chf99:
        movdqa          xmm6,  Locals.SavedXmm6[rsp]
        movdqa          xmm7,  Locals.SavedXmm7[rsp]
        movdqa          xmm8,  Locals.SavedXmm8[rsp]
        movdqa          xmm9,  Locals.SavedXmm9[rsp]

;
; Begin epilogue.  Deallocate stack space and return.
;

        add     rsp, LOCALS_SIZE
        ret

        IACA_VC_END


        NESTED_END CreateHistogramAvx512AlignedAsm_v4, _TEXT$00

        LEAF_ENTRY CreateHistogramAvx512AlignedAsm_v5, _TEXT$00
        ret
        LEAF_END CreateHistogramAvx512AlignedAsm_v5, _TEXT$00



; vim:set tw=80 ts=8 sw=4 sts=4 et syntax=masm fo=croql comments=\:;           :

end
