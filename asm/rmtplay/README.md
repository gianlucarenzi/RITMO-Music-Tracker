# The XEX with the RMT player (RITMO PLAYER)

The players of the "XEX Atari executable + RMT player" export of RITMO (File > Export As, script format `xex-rmt`):
the visualizer of RMTPLAY.COM of VERA_ATARI_PBI (`vera-tests/rmt/rmtplay.c`, the Atari screen only, without the
VERA parts) with the RMT player of a tracker driver. The tracker puts the module of the song after the program and
fills in NAME, AUTHOR and DATE.

```bash
cd asm/rmtplay
make            # build/rmtplay_v1_mono.xex, _v1_stereo, _v6_mono, _v6_stereo and build/rmt_xex.h
make install    # copy them to rmt/resources/players and src/rmt_xex.h
```

It needs cc65 (`cl65`, `ca65`, `ld65`), `make` and `awk`; the players are assembled by `../ca65/obx.sh`.

## The screen

`RITMO PLAYER`, `RMT4` or `RMT8 2 POKEY`, PAL / NTSC; NAME, AUTHOR, DATE (a line longer than 40 characters scrolls
after 5 seconds); one volume bar per channel (4, or 8 for an RMT8 song: L1-L4 on the first POKEY, R1-R4 on the second
one, $D210), coloured by a DLI; AUDF / AUDC of every channel and AUDCTL (two for RMT8); song line, row, speed, play
time. Keys: SPACE pause, R restart, ESC stop and exit.

## The files

| File | |
|------|-|
| `rmtplay.c` | the visualizer (cc65); the main loop fits in a frame next to the player: bars every frame, registers and text every other frame, only the cells that change, no multiplications or long divisions |
| `rmtxex.s` | the calls of the player, the deferred VBI (as many calls as the instrument speed, back to back), the speed of an NTSC song on a PAL machine and the other way round (a 6th tick every 5 frames, 5 ticks in 6 frames), the symbols of the player for the C side; `rmt_song`, `rmt_song_end`, `rmt_song_ntsc` are set by the tracker |
| `rmtdli.s` | the DLI that colours the bar rows (from VERA_ATARI_PBI) |
| `rmtplay.cfg` | ld65: zero page $82-$CA (the players use $CB-$DD), the font buffer at $2000, the program from $4000 with its BSS, the module right after it |
| `Makefile` | the four players and `rmt_xex.h` |

## The players

| | driver | player |
|-|--------|--------|
| `v1` | RMT 1.28 Unpatched (1, 2) | `asm/Legacy/rmt_xex.a65`: `rmt.a65` at $3400 (from $3000), every feature |
| `v6` | RMT 1.34 Patch16 (6) | `asm/Patch-16/rmtplayr.a65` at $3100 (`FEAT_IS_SIMPLEP`, variables from $2D40), its frequency tables at $3A00 (`TABLES`): the tracker copies there the tables it computes from the tuning |

`mono` for RMT4, `stereo` for RMT8 (`STEREOMODE` 1: 8 tracks, the right ones on the second POKEY). The XEX is the cc65
program (with its system check chunk at $2E00, run while the file loads) followed by the player; the tracker adds the
module from `RMTXEX_*_MODULE` (the end of the program) up to `RMTXEX_MODULE_LIMIT` ($B820: below the C stack, which the
Atari runtime puts under MEMTOP), about 24 KB.

## Checked

In atari800 (`-pokeyrec`, `-pokeyrec-stereo` with `-stereo`) against the SAP-R of RITMO with the same driver, frame
by frame (the state after the last of the calls of a frame): instrument speed 1, 2 and 4, mono and stereo (both
POKEYs), both drivers. With 8 tracks at instrument speed 4 the player takes up to half a frame (156 lines for
Patch16); the recorder of atari800 then sometimes samples POKEY in the middle of the calls of a frame or of their
register writes, the music itself is the stream of the SAP-R. `scripts/check-xex-rmt.py` checks the files the
scripting tests export.
