# The RMT player in a C program (cc65)

A song made with RITMO, played by the RMT player in a program written in C with
[cc65](https://github.com/cc65/cc65) (cc65, ca65, ld65). This folder is a complete example: `make` builds
`build/example.xex`, which plays `rmt/songs/rmt128/raster/gemx.rmt` and shows the frames played until a key is
pressed.

```bash
make                                    # build/example.xex
make SONG=path/to/song.rmt              # another song (a mono one, RMT4)
make SONG=path/to/stereo.rmt STEREO=1   # an 8-track song, on two POKEYs
```

It needs cc65 and RITMO, which exports the song (`RITMO=path/to/ritmo` if it is not `build-qt/out/ritmo` of this
repository). RITMO does not need to be built: a downloaded package works, for example
`make RITMO=~/Downloads/Ritmo-2.8-x86_64.AppImage`.

## The three parts

| Part | What it is | Where it goes |
|------|------------|---------------|
| The player | the Patch16 RMT player, `asm/ca65/Patch-16/rmtplayr.s`, assembled by ca65 at `$3100` (`FEAT_IS_SIMPLEP`) with its frequency tables at `$3A00` | `$2D40-$3FFF` (its variables from `$2D40`), zero page `$CB-$DD` |
| The module | the song, exported by RITMO as ca65 source | anywhere: the linker places it, in the program |
| The program | `main.c` and the small wrapper `rmt.s` (`rmt.h`) | from `$4000`, linked with `atari-rmt.cfg` |

The player is not relocatable: it has fixed addresses for its code, its variables and its zero page. It is
assembled on its own (`build/player.obx`) and appended to the program's XEX, so DOS loads it before the program
runs. The program only needs its address: `build/player.inc` has `RMT_PLAYER` (`$3100`), and the player's
vectors are `RMT_PLAYER+0` init (X/Y = the module, A = the first song line; it returns the instrument speed),
`+3` play and `+9` silence.

The module, on the other hand, is relocatable source: every pointer in it is a label, so ca65 and ld65 put it
wherever they like.

## Exporting the song as ca65 source

In RITMO: **File > Export As > "Relocatable ASM for RMTPlayer"**, **Assembler format: CA65 (cc65)**. Leave the
instruments, tracks and song lines together (no relocation): the module is then one block that the linker places.
The start label is the name the program uses: for C it starts with an underscore, `_rmt_module` is `rmt_module`
in C:

```c
extern const unsigned char rmt_module[];
```

From a script (or a Makefile, as here):

```
open song.rmt
export rmtplayer-asm song.asm relocate=none asmformat=ca65 startlabel=_rmt_module
```

The source is one `.proc` named by the start label, so its own labels (`__Track_00`...) do not collide with those
of the program or of a second song; the start label is exported. With relocation (the instruments, tracks or song
lines at addresses of your own) the parts begin with `.org ::LABEL`: define those labels as constants, for example
`ca65 -D RMT_SONG_LINES=$7000`.

The export also writes, at the top, the `FEAT_` section: the features of the player the song uses. The example
does not use it (the player has every feature, so any song plays). To make the player smaller, put those lines in
`asm/ca65/Patch-16/rmt_feat.s`, or pass them with `-D` to ca65.

## Playing it

```c
#include "rmt.h"

unsigned char speed = rmt_init(rmt_module);   // the song from its first line; the instrument speed (1..4)
rmt_vbi_on();                                 // the player on the deferred VBI: it plays by itself
...
rmt_vbi_off();                                // the VBI back, the POKEY silent
```

Every frame the VBI calls the player as many times as the instrument speed, back to back, as the XEX export of
RITMO does; `rmt_frames` counts the frames.

## Fading out

The player of the example is assembled with `FEAT_GLOBALVOLUMEFADE=1` (7 bytes more): a byte of the player is
subtracted from the volume of every channel, in the high nibble. C sees it as `rmt_global_volume_fade`:

```c
rmt_global_volume_fade = 0x00;   // full volume
rmt_global_volume_fade = 0x80;   // 8 steps lower
rmt_global_volume_fade = 0xF0;   // silent
```

`main.c` fades the song out this way when a key is pressed, one step every 4 frames. The byte is part of the
player's code (the operand of a `SBC`, `RMTGLOBALVOLUMEFADE` in `rmtplayr.s`), so its address is known only when
the player is assembled: the Makefile asks `obx.sh` for both symbols (`OBX_SYMBOLS="RASTERMUSICTRACKER
RMTGLOBALVOLUMEFADE"`) and writes one line of `player.inc` for each of them, by name:

```
RMT_PLAYER = $3100
RMT_GLOBALVOLUMEFADE = $3209
```

`rmt.s` exports `RMT_GLOBALVOLUMEFADE` as `_rmt_global_volume_fade` when `player.inc` has it. Without the feature
the player has no such byte: leave `RMTGLOBALVOLUMEFADE` out of `OBX_SYMBOLS` too (`obx.sh` would not find it) and
do not use the variable (the linker reports it as unresolved). A song for the other region (an NTSC song on a PAL machine) plays at
the speed of the machine: RITMO PLAYER (`asm/rmtplay/rmtxex.s`) shows how to add or drop a tick every few frames.

## The memory

`atari-rmt.cfg` is the `atari.cfg` of cc65 with three changes, as for RITMO PLAYER:

- the zero page of the C program ends at `$CA`: the player uses `$CB-$DD`;
- the program starts at `$4000`, above the player;
- `$2000-$2D3F`, below the player, is free memory (the `LOWBSS` segment).

The program, with the module, can reach `$B81F` (the C stack and the screen are above).

## Notes

- The player and the module must be of the same kind: this player (Patch16) plays modules made with the default
  tracker driver of RITMO, "RMT 1.34 Patch16". For a song made with "RMT 1.28 Unpatched", assemble
  `asm/ca65/Legacy/rmt_xex.s` instead (at `$3400`), as `asm/rmtplay/Makefile` does for its `v1` players.
- An older RMT player with a module of a newer driver, or the other way round, plays the song wrongly: when a song
  "does not sound right", check that the player and the tracker driver of the song match.
- `asm/rmtplay` is a bigger example of the same kind: RITMO PLAYER, the program of the "XEX + RMT player" export,
  with the volume bars, the registers and the info lines.
