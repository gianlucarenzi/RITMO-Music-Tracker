# RITMO Scripting

RITMO can run a script file from the command line: open a song,
set what the export dialogs would ask, export in any format, save. This is
meant for batch files, build scripts and CI, where the SAP, XEX, WAV and ASM
files of a song are produced from its `.rmt` source reproducibly, and for
repeated export sets while composing. The script format is the one of the
Windows `Ritmo.exe` and of the Java port of the RASTER-Music-Tracker
repository, so the same script runs in all of them.

## Running a script

```
ritmo /SCRIPT:build.rmtscript
```

`-script:<file>` and `--script=<file>` are accepted too. The program starts
without showing its window and exits with the script's exit code. The
configuration (`rmt.ini` and `tuning.ini`, kept in the user's settings -
`~/.config/ritmo-atari.org/ritmo.conf` on Linux) is read as for the window, so the
tuning shapes the exports exactly as in the tracker; it is not written back.

- **Linux:** no display is needed - without `QT_QPA_PLATFORM` set, a script
  run uses Qt's `offscreen` platform. Nothing is played: the sound output is
  not opened.
- **Windows:** `rmt.exe` is a windowed program, so `cmd.exe` returns to the
  prompt at once unless it is started with `start /wait` (in PowerShell:
  `Start-Process rmt.exe "/SCRIPT:build.rmtscript" -Wait`).
- **macOS:** `Ritmo.app/Contents/MacOS/ritmo /SCRIPT:build.rmtscript`.

Messages that would be boxes in the window are printed to the console, all
of them to stdout (on Windows a windowed program has no stderr that could
reach the log file); a question is answered "No". On Windows, when there is no console (started from Explorer or a
scheduler), they go to `<script>.log` next to the script.

Exit codes of the command line:

| Code | Meaning |
|---|---|
| 0 | every command succeeded |
| 1 | a command failed; the script stopped there and the message names the line |
| 2 | the script file could not be read or has a syntax error (the message names the line) |

## Syntax

- One command per line. Blank lines are ignored, `#` starts a comment (at
  the start of a line or before a token).
- Tokens are separated by blanks. A value with blanks is quoted:
  `"My Song.sap"`. Inside quotes, `\"` is a quote and `\\` a backslash.
- Options are written `name=value` after the command's arguments, in any
  order: `export sap out.sap author="Raster" subsongs=0`. A quoted value
  works after the `=` as well.
- Command names, option names and option values that are choices
  (`yes`/`no`, format names, driver names) are case-insensitive; other
  values (texts, labels, file names) keep their case.
- Yes/no options accept `yes`, `no`, `true`, `false`, `on`, `off`, `1`, `0`.
- Relative file names are relative to the script file's folder, so a
  script travels with its songs. Output files (`save`, `export`) go to the
  folder of `set output` instead when the script gave one; the folder is
  created when it is missing.
- An output file that exists already is an error, unless the script says
  `set overwrite yes` first.

## Commands

| Command | Meaning |
|---|---|
| `open <file>` | Opens a song. The format follows the extension: `.rmt`, `.txt` or `.rmw` (as File > Open). |
| `save <file>` | Saves the song as `.rmt`, `.txt` or `.rmw` by extension (as File > Save as). The RMT format's integrity check applies. |
| `export <format> <file> [name=value ...]` | Exports the song (as File > Export as). Formats and options below. |
| `set overwrite yes\|no` | Whether later `save`/`export` commands may replace existing files. Default `no`. |
| `set output <folder>` | Where later `save`/`export` commands write (relative to the script's folder; created when missing). Default: the script's folder. |
| `set ntsc yes\|no` | The Options dialog's NTSC setting (the video standard: 60 or 50 frames per second, and the POKEY clock). |
| `set driver <version>` | The Options dialog's tracker driver: `unpatched`, `unpatched-with-tuning`, `patch3`, `patch6`, `patch8`, `patch16` (the default), `patch-prince-of-persia`. |
| `set midi-touch-response yes\|no` | The Options dialog's MIDI "Touch response" (the velocity becomes the volume). Default: `rmt.ini`'s. |
| `set midi-volume-offset <0-15>` | The Options dialog's MIDI "Atari Volume Offset". Default: `rmt.ini`'s. |
| `set midi-note-off yes\|no` | The Options dialog's MIDI "Record note off". Default: `rmt.ini`'s. |
| `midi <status> <data1> [<data2>]` | Handles a MIDI message given as hex bytes exactly as one from the MIDI IN device would be (see the manual's "MIDI Input"): `midi 90 3C 64` is note on C-3, velocity 100, on channel 1 and records the note at the cursor; `midi CA 05` selects instrument 5. The window counts as focused. |
| `dump actions <file>` | Writes the program's command table - every menu item and toolbar button with its key and its prompt from `Rmt.rc` - as a Markdown table. The table is read from the running program's menus and toolbars, so it lists the keys the menu items really have (the tracker's own keys, handled in the editing code, appear only where a menu label shows them). A row marked ERROR is a menu label that shows another key than the item has and fails the command. |
| `dump notekeys <file>` | Writes the note keys of the QWERTY, AZERTY and QWERTZ keyboard layouts (which key plays which note) as keyboard pictures and Markdown tables . |
| `echo <text ...>` | Prints the text. |
| `quit` | Ends the script (implicit at its end). |

## Export formats and options

The format names are those of the Export dialog's file-type list. Every
option has the value the dialog would show when it opens, so a bare
`export sap out.sap` produces exactly what the dialog's OK produces with
untouched fields. The extension of the format is added to the file name
when it is missing.

| Format | File | Options |
|---|---|---|
| `stripped-rmt` | `.rmt` | `address` (the module's address, `$4000` or decimal; default: the loaded module's), `sfx` (SFX support), `gvf` (global volume fade), `nos` (no starting songline), `asmformat` = `xasm` or `atasm` |
| `asm` | `.asm` | `type` = `tracks` or `song` (tracks, or the whole song by song columns), `notes` = `index` or `freq`, `durations` = `notes`, `note-duration` or `duration-note`, `prefix` (the label prefix, empty = no labels; default `MUSIC`) |
| `sapr` | `.sapr` | `author`, `name`, `date` (default: the song's name, the song's author, today), `subsongs` (default: the subsongs found in the song) |
| `lzss` | `.lzss` | none. Writes the full tune and, when they are longer than 16 bytes, the `_INTRO.lzss` and `_LOOP.lzss` siblings. |
| `sap` | `.sap` | as `sapr` (a SAP of type B with the LZSS player of VU-Player V2; each songline listed in `subsongs` starts a subsong, `subsongs="00 10"` makes two) |
| `xex` | `.xex` | `text` (the screen text, up to 5 lines of 40 characters, `\n` between lines; default: the song's name, date and author lines the dialog proposes), `rasterbar` (yes/no), `shuffle` (yes/no), `region-auto` (yes/no), `color` (the rasterbar color, 0-255) |
| `xex-rmt` | `.xex` | `name`, `author`, `date` (the three info lines of RITMO PLAYER, up to 200 characters each; default: the song name up to its first `, `, the rest, its first year; a module without a name gets its file name). An XEX with the RMT player of the tracker driver (`unpatched` or `patch16`; other drivers stop the script) and the module; an RMT8 song plays on two POKEYs. |
| `rmtplayer-asm` | `.asm` | `startlabel` (default `RMT_SONG_DATA`), `relocate` (a comma list of `instruments`, `tracks`, `songlines`, or `none`), `instruments-label`, `tracks-label`, `songlines-label`, `asmformat`, `sfx`, `gvf`, `nos` |
| `wav` | `.wav` | none (16-bit, 44.1 kHz, 2 channels, the song once up to its loop point) |

Options that a format does not have, and values that are not valid, stop
the script with a message naming the line.

## Example

```
# Build the release assets of Delta
set overwrite yes
open Delta.rmt
export sap Delta.sap author="Raster" name="Delta" subsongs="0"
export xex Delta.xex text="DELTA BY RASTER\nJAVA PORT" rasterbar=no
export stripped-rmt Delta_stripped.rmt address=$5000
export wav Delta.wav
echo finished
```

```
ritmo /SCRIPT:Delta/build.rmtscript
```

## Environment variables

These variables let a build script run one and the same script file with
different outputs or settings, without editing it:

| Variable | Programs | Meaning |
|---|---|---|
| `RMT_SCRIPT_OUTPUT=<folder>` | all | The output folder for every `save`/`export`; overrides the script's `set output`. |
| `RMT_SCRIPT_LOG=<file>` | all | Writes the messages to this file instead of the console or `<script>.log`. |
| `RMT_SCRIPT_SHOW_WINDOW=1` | all | Shows the window during the script run (normally it stays hidden), to watch what a script does. |
| `RMT_CONFIG_DIR=<folder>` | all | Reads the configuration from `ritmo.ini` and `tuning.ini` in this folder (text files, as RMT wrote them) instead of the user's settings; a missing file means the defaults. The same in the window, which also saves them there. |

A script run reads the configuration and never writes it, not even the
defaults on a machine that has none yet: `set` changes the settings of the run
only.

## Tests

`test-resources/scripts` holds two reference scripts (`delta.rmtscript`: the
mono reference song through every save and export, once with the defaults and
once with every option set; `midi.rmtscript`: MIDI messages recorded into it).
`scripts/test-scripting.sh [path/to/ritmo]` runs both twice into separate
folders and checks the exit codes, that the files exist and that the two runs
are byte-identical; it also checks the exit codes 1 and 2. The scripts are the
ones the Windows program and the Java port compare their exports with, so the
output folders can be compared with those programs' as well (WAV files
excepted: 8-bit in `Ritmo.exe`, 16-bit in the Java port).

## Not available as script commands

Importing (MOD, TMC), the instrument files (`.rti`), the track files and
`New` - the tracker's interactive functions - and, in this program, "Tools >
Run Script" in the window. The command set follows the
export workflow; further commands can be added if a need arises.
