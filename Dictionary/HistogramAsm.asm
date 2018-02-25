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

OP_NEQ equ 4

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

    ReturnAddress   dq  ?
    HomeRcx         dq  ?
    HomeRdx         dq  ?
    HomeR8          dq  ?
    HomeR9          dq  ?
Locals ends

;
; Exclude the return address onward from the frame calculation size.
;

LOCALS_SIZE  equ ((sizeof Locals) + (Locals.ReturnAddress - (sizeof Locals)))

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
;   r10 - Byte counter.
;
;   r11 - Byte counter.
;

        mov     r8, rdx                             ; Load 1st histo buffer.
        lea     r9, Histogram.Histogram2[r8]        ; Load 2nd histo buffer.
        mov     rdx, String.Buffer[rcx]             ; Load string buffer.
        mov     ecx, String.Length[rcx]             ; Load string length.
        shr     ecx, 6                              ; Divide by 64 to get loop
                                                    ; iterations.


        ;xor             rax, rax
        ;mov             al, 011h
        ;kmovb           k1,

        mov             rax, 1111111111111111h
        kmovq           k1, rax
        vpmovm2b        zmm1, k1
        vpsubb          zmm21, zmm1, zmm1

        kshiftlq        k2, k1, 1
        vpmovm2b        zmm2, k2
        vpsubb          zmm22, zmm2, zmm1

        kshiftlq        k3, k1, 2
        vpmovm2b        zmm3, k3

        kshiftlq        k4, k1, 3
        vpmovm2b        zmm4, k4

        jmp             Chb30

        kshiftlb        k4, k4, 1
        vpmovm2b        zmm2, k4

        kshiftlb        k3, k1, 2
        kshiftlb        k4, k1, 3

        vpmovm2w        zmm1, k1
        vpmovm2d        zmm2, k1

        mov             ax, 0ffh
        kmovd           k2, eax
        vpmovm2b        zmm3, k2
        vpmovm2w        zmm4, k2
        vpmovm2d        zmm5, k2

        ;mov             rdx, rax
        ;shr             rdx, 1
        ;not             rdx

        kshiftld        k2, k1, 1
        kshiftld        k3, k1, 2
        kshiftld        k4, k1, 3

        knotd           k5, k1
        knotd           k6, k2
        knotd           k7, k3

        mov             rax, 1
        movd            xmm0, rax
        vpbroadcastb    zmm4, xmm0

        mov             eax, -1
        movd            xmm0, rax
        vpbroadcastd    zmm5, xmm0

        mov             ax, 31
        not             ax
        movd            xmm0, rax
        vpbroadcastd    zmm6, xmm0

        mov             rax, 255
        movd            xmm0, rax
        vpbroadcastb    zmm7, xmm0

        xor             rax, rax
        kxnorw          k2, k2, k2

        align 16

Chb30:  vmovntdqa       zmm0, zmmword ptr [rdx]     ; Load 64 bytes into zmm0.


Chb35:  vpandd          zmm5, zmm1, zmm0
        vpandd          zmm6, zmm2, zmm0
        vpandd          zmm7, zmm3, zmm0
        vpandd          zmm8, zmm4, zmm0

        ;mov             rax, 0001000100010001h
        mov             rax, 0001000100010001h
        kmovq           k1, rax
        vpmovm2b        zmm1, k1


        vpermb          zmm20, zmm10, zmm6

        vprord          zmm20, zmm6, 1
        vprord          zmm21, zmm6, 2
        vprord          zmm22 {k2}, zmm6, 1
        vprord          zmm23 {k3}, zmm6, 1
        vprord          zmm24 {k4}, zmm6, 1

        ;xor             eax, eax
        ;add             eax, 1
        ;kmovb           k1, eax
        ;vpbroadcastmw2d zmm9, k1

        ;vpsrlvd         zmm10, zmm6, zmm0
        ;vpsrlvd         zmm11, zmm6, zmm0

        ;xor             rax, rax
        ;add             rax, 1

        vpsrlvd         zmm11, zmm6, zmm1

        vpsravd         zmm10 {k2}, zmm6, zmm2
        vpsrlvd         zmm11 {k2}, zmm6, zmm2
        vpsrlvd         zmm12 {k0}, zmm6, zmm2
        ;vpsravd         zmm11, zmm7, 2
        ;vpsravd         zmm12, zmm8, 3

        vpconflictd     zmm20, zmm5
        vpconflictd     zmm21, zmm10
        vpconflictd     zmm22, zmm11
        vpconflictd     zmm23, zmm12

        vptestmd        k4, zmm20, zmm20
        vptestmd        k5, zmm21, zmm21
        vptestmd        k6, zmm22, zmm22
        vptestmd        k7, zmm23, zmm23

        vpandd          zmm6 {k2} {z}, zmm1, zmmword ptr [rdx]

        vpandd          zmm1 {k1} {z}, zmm7, zmmword ptr [rdx]
        vpandd          zmm1 {k1} {z}, zmm7, zmmword ptr [rdx]
        vpandd          zmm2 {k2} {z}, zmm7, zmm0
        vpandd          zmm3 {k3} {z}, zmm7, zmm0


;
; Top of the histogram loop.
;

        align 16

;
; Load the first 64 bytes into ymm0.  Duplicate the lower 16 bytes in xmm0 and
; xmm1, then extract the high 16 bytes into xmm2 and xmm3.
;

Chb50:  vmovntdqa       zmm3, zmmword ptr [rdx]     ; Load 64 bytes into zmm0.
        vpconflictd     zmm0, zmm3
        vpandd          zmm8, zmm7, ymmword ptr [rdx]
        kxnorw          k1, k1, k1
        vmovaps         zmm2, zmm4
        vpxord          zmm1, zmm1, zmm1
        vpgatherdd      zmm1 {k1}, [r8+zmm9*4]
        vptestmd        k1, zmm0, zmm0
        kortestw        k1, k1
        je              Chb60

        vplzcntd        zmm0, zmm0
        vpsubd          zmm0, zmm6, zmm0

;
; Handle conflicts.
;

Chb55:  vpermd          zmm8 {k1} {z}, zmm2, zmm0
        vpermd          zmm0 {k1},     zmm0, zmm0
        vpaddd          zmm2 {k1},     zmm2, zmm8
        vpcmpd          k1 {k2}, zmm5, zmm0, OP_NEQ
        kortestw        k1, k1
        jne             Chb55

;
; No conflicts; update counts.
;

Chb60:  vpaddd          zmm0, zmm2, zmm1
        kxnorw          k1, k1, k1
        vpscatterdd     [r8+zmm3*4]{k1}, zmm0

;
; End of loop.  Update loop counter and determine if we're finished.
;

        sub         ecx, 1                              ; Decrement counter.
        jnz         Chb50                               ; Continue loop if != 0.

;
; We've finished creating the histogram.  Merge the two histograms 64 bytes at
; a time using YMM registers.
;

        mov         ecx, 16                             ; Initialize counter.

        align 16

Chb75:  vmovntdqa   ymm0, ymmword ptr [r8+rax]      ; Load 1st histo  0-31.
        vmovntdqa   ymm1, ymmword ptr [r8+rax+20h]  ; Load 1st histo 32-63.
        vmovntdqa   ymm2, ymmword ptr [r9+rax]      ; Load 2nd histo  0-31.
        vmovntdqa   ymm3, ymmword ptr [r9+rax+20h]  ; Load 2nd histo 32-63.

        vpaddd      ymm4, ymm0, ymm2                ; Add  0-31 counts.
        vpaddd      ymm5, ymm1, ymm3                ; Add 32-63 counts.

        vmovntdq    ymmword ptr [r8+rax], ymm4      ; Save counts for  0-31.
        vmovntdq    ymmword ptr [r8+rax+20h], ymm5  ; Save counts for 32-63.

        add         rax, 40h                        ; Advance to next 64 bytes.
        sub         ecx, 1                          ; Decrement loop counter.
        jnz         short Chb75                     ; Continue if != 0.

;
; Indicate success.
;

        mov rax, 1

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


        NESTED_END CreateHistogramAvx512AlignedAsm, _TEXT$00


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
