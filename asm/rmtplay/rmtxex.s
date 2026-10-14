; rmtxex.s - the RMT player of the XEX export of RITMO (rmtplay.c): the calls of the player, the VBI, the speed of an
; NTSC song on a PAL machine and the other way round, and the symbols of the player for the C side.
;
; The player is one of the RMT players of asm/ (RMT 1.28 at $3400, Patch16 at $3100), assembled at its own address
; and loaded from the same XEX; build/player_*.inc (Makefile) has the addresses of its entry points and variables:
;   RMT_PLAYER                       the vectors: +0 init (X/Y = module, A = song line; returns the instrument
;                                    speed), +3 play, +9 silence
;   RMT_P_SONG                       zero page pointer to the next song line
;   RMT_ABEAT, RMT_MAXTRACKLEN, RMT_SPEED   row in the track, rows per track, frames per row
;   RMT_AUDF, RMT_AUDC, RMT_AUDCTL   the channels the player computed (RMT_AUDCTL2: the right POKEY, 8 tracks)
;
; Every frame the deferred VBI calls the player as many times as the instrument speed, back to back (as RMTPLAY of
; VERA_ATARI_PBI and RmtSkeleton do): tempo and instruments are right, POKEY changes once per frame. With 8 tracks the
; player writes the second POKEY ($D210) itself.
;
; The tracker (SongExporter.cpp) patches: rmt_song / rmt_song_end (the module, after the program), rmt_song_ntsc
; (1 = NTSC song) and the three info lines of rmtplay.c.

        .include "player.inc"

        .export _rmt_init, _rmt_vbi_on, _rmt_vbi_off, _rmt_frames
        .export _rmt_song, _rmt_song_end, _rmt_song_ntsc
        .exportzp _rmt_p_song := RMT_P_SONG
        .export _rmt_abeat := RMT_ABEAT, _rmt_maxtracklen := RMT_MAXTRACKLEN, _rmt_speed := RMT_SPEED
        .export _rmt_chan_audf := RMT_AUDF, _rmt_chan_audc := RMT_AUDC, _rmt_audctl := RMT_AUDCTL
.ifdef RMT_AUDCTL2
        .export _rmt_audctl2 := RMT_AUDCTL2
.endif

SETVBV  = $E45C
XITVBV  = $E462
VVBLKD  = $0224
PALNTS  = $62                   ; XL/XE OS: 0 = NTSC, 1 = PAL

rmt_play    = RMT_PLAYER + 3
rmt_silence = RMT_PLAYER + 9

        .segment "DATA"
_rmt_song:      .word $FFFF     ; the module (set by the tracker)
_rmt_song_end:  .word $FFFF     ; first byte after it
_rmt_song_ntsc: .byte 0         ; 1 = the song is meant for 60 Hz

        .segment "BSS"
_rmt_frames:    .res 2          ; frames played (VBI counter)
ispeed:         .res 1          ; player calls per frame (instrument speed, 1..4)
ticks:          .res 1
adjust:         .res 1          ; 0 none, 1 = PAL machine, NTSC song: a 6th tick every 5 frames,
                                ; 2 = NTSC machine, PAL song: 5 ticks in 6 frames
phase:          .res 1          ; frame in the 5 or 6 of the adjustment
oldvbd:         .res 2
vbion:          .res 1

        .segment "CODE"

; unsigned char __fastcall__ rmt_init(const void *module): the instrument speed (1 = once per frame)
_rmt_init:
        pha                     ; A/X = module lo/hi -> X/Y for the player
        txa
        tay
        pla
        tax
        lda #0                  ; from song line 0
        jsr RMT_PLAYER
        cmp #1                  ; 1..4, the RMT range
        bcs @lo
        lda #1
@lo:    cmp #5
        bcc @hi
        lda #4
@hi:    sta ispeed
        ldx #0
        rts

; void rmt_vbi_on(void): the player on the deferred VBI
_rmt_vbi_on:
        lda vbion
        bne @done
        ldx #0                  ; the adjustment between the song and the machine
        lda PALNTS
        beq @ntscm
        lda _rmt_song_ntsc
        beq @set
        ldx #1                  ; PAL machine, NTSC song
        bne @set
@ntscm: lda _rmt_song_ntsc
        bne @set
        ldx #2                  ; NTSC machine, PAL song
@set:   stx adjust
        lda #0
        sta phase
        lda VVBLKD
        sta oldvbd
        lda VVBLKD+1
        sta oldvbd+1
        ldy #<vbi
        ldx #>vbi
        lda #7                  ; 7 = deferred VBI vector
        jsr SETVBV
        inc vbion
@done:  rts

; void rmt_vbi_off(void): VBI back, POKEY silent
_rmt_vbi_off:
        lda vbion
        beq @done
        ldy oldvbd
        ldx oldvbd+1
        lda #7
        jsr SETVBV
        lda #0
        sta vbion
        jsr rmt_silence
@done:  rts

; one tick of the song: ispeed calls, back to back
play_tick:
        lda ispeed
        sta ticks
@l:     jsr rmt_play
        dec ticks
        bne @l
        rts

vbi:
        inc _rmt_frames
        bne @nohi
        inc _rmt_frames+1
@nohi:  lda adjust
        beq @one
        inc phase
        cmp #1
        bne @ntsc
        lda phase               ; PAL machine: every 5th frame one tick more
        cmp #5
        bcc @one
        lda #0
        sta phase
        jsr play_tick
        jmp @one
@ntsc:  lda phase               ; NTSC machine: every 6th frame no tick
        cmp #6
        bcc @one
        lda #0
        sta phase
        jmp XITVBV
@one:   jsr play_tick
        jmp XITVBV
