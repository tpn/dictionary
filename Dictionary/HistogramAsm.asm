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
    Count   dd 1024 dup (?)
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
Histogram typedef CHARACTER_HISTOGRAM

        NESTED_ENTRY CreateHistogramAvx2AlignedAsm, _TEXT$00
        END_PROLOGUE
        ret
        NESTED_END CreateHistogramAvx2AlignedAsm, _TEXT$00

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
        jz      Cha99                                   ; Yes, abort.
        test    rdx, rdx                                ; Is rdx NULL?
        jz      Cha99                                   ; Yes, abort.
        test    r8, r8                                  ; Is r8 NULL?
        jz      Cha99                                   ; Yes, abort.

;
; Verify the string is at least 32 bytes long.
;
        mov     r9, 32                                  ; Initialize r9 to 32.
        cmp     String.Length[rcx], r9d                 ; Compare Length to 32.
        jl      Cha99                                   ; String is too short.

;
; Ensure the incoming string and histogram buffers are aligned to 32-byte
; boundaries.
;

        dec     r9                                      ; r9 is now 31.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Cha99                                   ; No, abort.
        test    String.Buffer[rcx], r9                  ; Is string aligned?
        jnz     Cha99                                   ; No, abort.

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

Cha99:  ret

        LEAF_END CreateHistogramOld, _TEXT$00



; vim:set tw=80 ts=8 sw=4 sts=4 et syntax=masm fo=croql comments=\:;           :

end
