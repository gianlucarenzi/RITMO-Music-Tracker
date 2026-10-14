; rmtdli.s - display list interrupt that colours the rows of the RMTPLAY
; volume bars (rmtplay.c).
;
; The bars are ANTIC mode 4 rows (multicolour text): their background is
; COLBK, the bar pixels use COLPF0. The DLI bit is set on the display list
; line before each bar row: each DLI loads the next entry of rmt_dli_pf0 into
; COLPF0 at the start of the next scan line. The text rows (ANTIC mode 2) do
; not use COLPF0, so nothing has to be given back after the bars. After the
; last entry the index goes back to 0, so every frame starts again from the
; first row; the C side enables the DLIs only below the bars (VCOUNT), so the
; first DLI it sees is the first one of a frame.

        .export _rmt_dli, _rmt_dli_pf0, _rmt_dli_count, _rmt_dli_idx

COLPF0  = $D016
WSYNC   = $D40A

DLI_MAX = 16

        .segment "DATA"
_rmt_dli_pf0:   .res DLI_MAX
_rmt_dli_count: .byte 0         ; entries used
_rmt_dli_idx:   .byte 0         ; next entry

        .segment "CODE"
_rmt_dli:
        pha
        txa
        pha
        ldx _rmt_dli_idx
        lda _rmt_dli_pf0,x
        sta WSYNC
        sta COLPF0
        inx
        cpx _rmt_dli_count
        bcc @keep
        ldx #0
@keep:  stx _rmt_dli_idx
        pla
        tax
        pla
        rti
