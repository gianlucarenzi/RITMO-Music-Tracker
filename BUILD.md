# Building RITMO with CMake

RITMO is a **Qt** program (Qt6, or Qt 5.15): the same build on Linux, Windows
(MinGW / MSYS2) and macOS. `-DRMT_BUILD_CORE_ONLY=ON` builds the engine and
the audio/MIDI backends only, without Qt.

| Platform | Build | GUI | Section |
|----------|-------|-----|---------|
| Linux / POSIX | `cmake -B build-qt` | Qt6 | [Qt6 frontend](#official-build-qt6-frontend-linux--posix) |
| Windows | MSYS2 MinGW64 | Qt6 | [Windows with MinGW native](#windows-with-mingw-native-msys2) |
| Windows / Linux | MinGW, core only | none (engine, backends, tests) | [MinGW](#mingw-engine-and-backends-only) |

---

## Official build: Qt6 frontend (Linux / POSIX)

### Prerequisites

- CMake 3.25+ and a C++20 compiler (GCC 10 or later)
- Qt 6 (Core, Widgets); Qt 5.15 still works where Qt6 is not installed
  (`-DRMT_QT_MAJOR=5` or `6` chooses the version)
- PortAudio, for sound (without it the tracker runs silent)
- RtMidi, for MIDI input (without it there are no MIDI IN devices)

```bash
sudo apt install cmake qt6-base-dev portaudio19-dev librtmidi-dev
```

### Build and run

```bash
cmake -B build-qt -DCMAKE_BUILD_TYPE=Release
cmake --build build-qt -j
./build-qt/out/ritmo song.rmt
```

The build is free of compiler warnings with GCC 10 (`-Wall -Wextra`).

### Loading and playing a song

A song can be loaded from the command line at start-up:

```bash
./build-qt/out/ritmo legacy/rmt_128/songs/thrust.rmt
```

Or via **File → Load…** (`Ctrl+L`) from the menu bar. Give the window focus
and press **F5** to play.

The file dialogs are Qt `QFileDialog`s: `CFileDialog::DoModal()` asks the
frontend (`IRmtHost::FileDialog`), so the code in `IO_Song.cpp` is
unchanged. They work for songs (Load, Save, Save As: RMT, TXT, RMW),
instruments (RTI) and tracks (TXT), keep the last folder, propose the current
file name and return the chosen file type like `CFileDialog`. The filters match
upper case extensions too (`*.rmt *.RMT`), and a name typed without extension
gets the one of the chosen type. Import works for MOD and TMC, Export As
for all formats (see [Status](#status)).

### Configuration

The configuration and the tuning (`ritmo.ini` and `tuning.ini`, as text files
next to the program in the earlier versions) are kept by the Qt frontend in `QSettings`
(`qt/RmtQtSettings.cpp`), one key per line of those files in the groups
`rmt` and `tuning`. They belong to the user, not to the program folder, so
updating or reinstalling RMT keeps them:

| System | Where |
|--------|-------|
| Linux | `~/.config/ritmo-atari.org/ritmo.conf` (`$XDG_CONFIG_HOME`) |
| Windows | registry, `HKEY_CURRENT_USER\Software\ritmo-atari.org\ritmo` |
| macOS | `~/Library/Preferences/org.ritmo-atari.ritmo.plist` |

The first start with nothing saved takes over the `rmt.ini` / `tuning.ini`
of an earlier version next to the program, if there are any, else it saves
the defaults (without a "Could not find" message).

### Menu bar

All 7 top-level menus from `Rmt.rc` (`IDR_MAINFRAME MENU`) are present:

| Menu | Contents |
|------|---------|
| **File** | New, Load, Reload, Save, Save As, Import, Export As, Exit |
| **Edit** | Undo, Redo, Clear Undo & Redo history |
| **Track** | Copy/Paste/Cut/Delete, Info, loop tools, renumber, load/save track, cleanup |
| **Block** | Backup, Copy/Paste/Cut/Delete, Paste special (submenu), Effects, Select all |
| **Instrument** | Copy/Paste/Cut/Delete, Paste special (submenu, 9 items), Info, renumber, load/save, cleanup |
| **Song** | Line operations, 4/8 channel switch, order change, length, optimizations |
| **View** | Configuration, Tuning, toolbar toggles, Play time counter, Volume analyzer, Pokey regs, Instrument active help |
| **Help** | Help Topics, Online Help, About |

Menu items are enabled/disabled automatically via `ON_UPDATE_COMMAND_UI`
handlers (implemented by `QtCCmdUI`, triggered on `QMenu::aboutToShow`).

### Status

- ✅ the main screen, drawn by the original code (tracks, song, instrument,
  info, POKEY registers), window resize, the 16 ms screen timer. The widget
  shows the view's own bitmap (`m_mem_dc`) directly: `RmtQtBridge::Paint()`
  runs `CRmtView::OnDraw()` without its final `StretchBlt`, and the scaling
  (`SCALEPERCENTAGE` > 100) is done by `QPainter` (nearest neighbour; at
  scaled sizes a duplicated row/column may land one pixel apart from the Windows
  version of RMT)
- ✅ keyboard (navigation, editing keys), mouse buttons, wheel, cursors
- ✅ a song given on the command line is loaded
- ✅ full menu bar (7 menus, 81 actions, all with handlers; auto-tested)
- ✅ file dialogs (Load / Save / Save As, instrument and track load/save,
  the file choice of Import and Export As), `QFileDialog`
- ✅ File → New (`IDD_FILENEW`): track length 1-256 and mono/stereo, with the
  confirmation for tracks longer than 64 lines
- ✅ File → Import: the MOD and TMC options (`IDD_IMPORTMOD`, `IDD_IMPORTTMC`)
  and the "Import of module finished" dialogs (`IDD_IMPORTMODFINISHED`,
  `IDD_IMPORTTMCFINISHED`); checked with `rmt/imports/axel_f.mod`
  (ProTracker) and `rmt/imports/404_Error.tmc` (Theta Music Composer)
- ✅ File → Export As: the options of stripped RMT (`IDD_EXPORT_STRIPPED_RMT`,
  address, SFX and RMTFEAT definitions updated as they change), ASM simple
  notation (`IDD_EXPORT_ASM`), relocatable ASM for RmtPlayer
  (`IDD_EXPORT_RMTPLAYER_ASM`), SAP-R and SAP (`IDD_EXPSAP`) and XEX
  (`IDD_EXPMSX`, screen text preview, rasterbar color); LZSS has no options
- ✅ WAV export (44.1 kHz, 8 bit, stereo, one pass of the song up to its loop
  point): written with `std::ofstream` (`WaveFile.cpp`, the `mmio*()` API is
  Windows only) and rendered with the built-in POKEY; mono and stereo songs.
  Checked against the sound of the tracker (`RMT_AUDIO_DUMP`): loudness
  correlation 0.97 (gemx.rmt), 0.94 / 0.97 left / right (shorty_noises.rmt)
- ✅ View → Configuration (`IDD_CONFIG`) with its Paths... dialog
  (`IDD_PATHS`, folder choice with `QFileDialog`); the MIDI IN devices are
  the RtMidi input ports
- ✅ the other dialogs: block effects (`IDD_EFFECTS`, Try / Restore /
  Play/Stop), song columns' order (`IDD_SONGTRACKSORDER`), change of all the
  instrument occurrences (`IDD_INSTRCHANGE` with `IDD_CHANNELSSELECT`), insert
  copy or clone of song lines, maximal track length, renumber tracks /
  instruments, tracks loading, tuning (`IDD_TUNING`, Test now / Reset), and the
  octave, volume and instrument popups of the info line; all the dialogs of
  the tracker are in Qt
- ✅ 6502 and POKEY emulation built in (`src/emu`), the only one on every platform (no
  `sa_c6502.dll` / `apokeysnd.dll` / `sa_pokey.dll` is loaded, on Windows too): the tracker driver
  runs, notes and instruments play inside the engine
- ✅ playback with sound: the song timer (`timeSetEvent`, a thread; a timer
  re-created from its own tick keeps the deadline, so the tempo does not
  drift) runs `CSong::TimerRoutine()`, and the DirectSound buffer the sound
  code streams to is played by PortAudio with real cursors (`CompatAudio.cpp`,
  `RMT_HAVE_PORTAUDIO`). A re-created timer waits for the tick that created
  it to return, and a timer more than 200 ms late restarts from now instead
  of catching up: before, a stall of more than one tick (window move, load,
  debugger) let two ticks run at once, the timers doubled at every stall and
  the process ended at 100% on all cores (thousands of threads, then an
  abort). `CSongTimer::StopTimer()` is called on every exit (`main-qt.cpp`)
  and no tick can start a new timer once it has run, so the song timer no
  longer runs into the destruction of `g_Song` (segfault on exit)
- ✅ CPU (offscreen, gemx.rmt): about 50% of one core idle and while playing
  (was 78% / 66%), almost all of it the 60 fps redraw; `CDC::BitBlt` copies
  clipped rows with `memmove`, `CDC::StretchBlt` precomputes its columns
- ✅ MIDI input: the RtMidi input ports (ALSA sequencer) are the MIDI IN
  devices of the configuration (`midiInGetNumDevs()` / `midiInGetDevCaps()`
  in `RmtMidiRt.cpp`, the ALSA `client:port` numbers left out of the name so
  the saved device is found again), and `CRmtMidi` opens the chosen one with
  `RtMidiIn`: each message goes to `CSong::MidiEvent()` from the RtMidi
  thread, as from the winmm callback of `RmtMidi.cpp`. Checked with the ALSA
  "Midi Through" port (`snd-seq-dummy`): notes on MIDI channel 16 are
  written into the track in edit mode. MIDI on/off (`ID_MIDIONOFF`) is a
  button of the main toolbar
- ✅ toolbars: the main and block toolbars of `CMainFrame::OnCreate()`
  (`IDR_MAINFRAME`, `IDR_TOOLBARBLOCK`) with the icons of their embedded
  bitmaps, the tooltips and status texts of the string table and the
  "Insert note spacing" combo (`g_linesafter`, also followed when changed with
  Ctrl+numpad +/-); the buttons are checked and enabled by their
  `ON_UPDATE_COMMAND_UI` handlers every 100 ms, the
  icons follow the interface size. View → Main toolbar / Block toolbar /
  Status Bar show and hide them (`ShowControlBar()`), kept in the
  configuration

### Testing without a display

`RMT_QT_GRAB=shot.png` saves the window after 1 s (`RMT_QT_GRAB_MS`) and
quits, `RMT_QT_KEYS=108,106` first presses keys (Linux evdev codes, 63 = F5
play), message boxes are answered automatically; `RMT_AUDIO_DUMP=out.wav`
replaces the sound card with a thread that takes the sound buffer in real time
and writes it to a WAV file:

```bash
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=shot.png ./build-qt/out/ritmo song.rmt
QT_QPA_PLATFORM=offscreen RMT_AUDIO_DUMP=play.wav RMT_QT_KEYS=63 RMT_QT_GRAB_MS=6000 \
    RMT_QT_GRAB=shot.png ./build-qt/out/ritmo song.rmt
```

`RMT_QT_COMMANDS` then triggers the menu actions of the given command IDs
(`resource.h`, decimal or `0x` hex; IDs without a menu item, such as
`ID_PLAY1` = 32796, are sent as `WM_COMMAND`), and `RMT_QT_FILEDIALOG` answers the file
dialogs in turn (the file type is taken from the extension, or given as
`file@N` with the 1-based filter index, e.g. `song.asm@7` for the relocatable
ASM export; none left: cancel). Load a song, save it as TXT, load the TXT and save it as RMT:

```bash
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=1 RMT_QT_GRAB_MS=3000 \
    RMT_QT_COMMANDS=0xE101,0xE104,0xE101,0xE104 \
    RMT_QT_FILEDIALOG=gemx.rmt,/tmp/g.txt,/tmp/g.txt,/tmp/g2.rmt ./build-qt/out/ritmo
cmp gemx.rmt /tmp/g2.rmt     # identical
```

`RMT_QT_DIALOG=dialog.png` shows the other dialogs too (without it they are
cancelled in test runs): each is saved after 0.5 s (the first to
`dialog.png`, the next ones to `dialog-2.png`, `dialog-3.png`...) and
confirmed the way a user does it (for the import result: check "I
understand", then OK), through the same checks as a click on OK:

```bash
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=shot.png RMT_QT_GRAB_MS=3000 \
    RMT_QT_COMMANDS=0xE100 RMT_QT_DIALOG=new.png ./build-qt/out/ritmo song.rmt
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=shot.png RMT_QT_GRAB_MS=5000 \
    RMT_QT_COMMANDS=32856 RMT_QT_FILEDIALOG=rmt/imports/axel_f.mod \
    RMT_QT_DIALOG=import.png ./build-qt/out/ritmo      # import.png, import-2.png
```

`ID_FILE_NEW` = `0xE100`, `ID_FILE_OPEN` = `0xE101`, `ID_FILE_SAVE_AS` = `0xE104`, `ID_FILE_IMPORT` =
32856, `ID_FILE_EXPORT_AS` = 32773, `ID_INSTR_LOAD` / `ID_INSTR_SAVE` = 32772
/ 32771, `ID_TRACK_LOAD` / `ID_TRACK_SAVE` = 32888 / 32889.

Test runs change the configuration like the user does: give them their own
with `XDG_CONFIG_HOME=/tmp/rmt-test` (Linux) to keep yours as it is.

MIDI input through the ALSA "Midi Through" port (client 14, module
`snd-seq-dummy`) with `MIDI_IN=Midi Through:Midi Through Port-0` in the
`[rmt]` group of `rmt.conf`: while the tracker runs, `aplaymidi -p 14:0 notes.mid` plays a MIDI
file into it (notes on channel 16 are recorded in edit mode, `aconnect -l`
shows the `RMT` client connected to 14:0).

`RMT_QT_MENU_TEST=1` (use with `RMT_QT_GRAB=1`, a long enough `RMT_QT_GRAB_MS`, which
ends the run, and `QT_QPA_PLATFORM=offscreen`) triggers all leaf menu actions
programmatically and exits 0 if every action has a registered handler, 1 otherwise:

```bash
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=1 RMT_QT_GRAB_MS=20000 RMT_QT_MENU_TEST=1 \
    ./build-qt/out/ritmo
# RMT_QT_MENU_TEST: triggered 162 menu actions
# RMT_QT_MENU_TEST: PASS
```

The menu and the keys are defined in `src/Rmt.rc` only (`IDR_MAINFRAME MENU` and
`IDR_MAINFRAME ACCELERATORS`): CMake turns them into tables at configure time
(`cmake/GenerateRcTables.cmake`) and the frontend builds its menu bar and its
shortcuts from them. Scripts: `ritmo /SCRIPT:<file>` ([doc/rmt_scripting.md](doc/rmt_scripting.md)),
checked by `scripts/test-scripting.sh`, like the menu test on every push by the
workflow `.github/workflows/test.yml`. `scripts/make-docs.sh` regenerates the command
table, the note keys and the HTML manual (`doc/rmt_en.md`) from the program.

Checked this way with gemx.rmt: after F5 the time counter shows 5.50 s at
5.5 s, the sound correlates 0.985 (chroma) and 0.989 (loudness, 10 ms steps)
with `rmtplay`, with the same delay at the start and at the end (no drift).

The window opens at 1366x768 (reduced to the screen, `qt/RmtQtScreen.h`); the offscreen platform has no
screen to limit it, which is what the tests and the screenshots use. `--scale=N` (100-300) sets the interface
size of a session without changing the settings.

`RMT_QT_MENU_GRAB=prefix` saves every top level menu as `prefix-<name>.png` (with the
screenshot hook). The user manual (`doc/manual`, LaTeX, built to PDF by `doc/manual/build.sh`) takes
all its screenshots with these hooks: `doc/manual/make-screenshots.sh`.

`RmtCoreTest --screenshot out.ppm [song.rmt]` draws the main screen without Qt;
`RmtCoreTest --play song.rmt frames regs.txt [out.wav]` plays a song with the
engine (one `CSong::TimerRoutine()` per frame), writes the POKEY registers of
every frame and the sound of the built-in POKEY (see
[Linux native core build](#linux-native-core-build)). `RMT_DRIVER=1..7` chooses
the tracker driver (the numbers of `TrackerDriverVersion.h`, default 6 = Patch16);
without a window a message box answers Yes, so a song with the AUTOFILTER plays
with the Unpatched driver when the driver is Patch16.

### How it works

The tracker GUI is the code of RMT (`RmtView.cpp`, `RmtDoc.cpp` and the
GUI-shared drawing code) compiled against `src/CompatTypes.h`, a small
layer with the Windows-style classes it was written with: a software device context (`CDC`,
`CBitmap`, bitmaps of `src/res` compiled in by `cmake/EmbedResources.cmake`),
message maps that build a real command table, and `CWnd`/`CView` whose window
operations go to an `IRmtHost`. `src/qt/` implements that host with Qt6:

| File | |
|------|---|
| `qt/main-qt.cpp` | start-up and main window; `RMT_QT_GRAB` / `RMT_QT_MENU_TEST` test hooks |
| `qt/RmtQtFrontend.cpp` | `RmtMainWindow` (menu bar, `QtCCmdUI`), `RmtViewWidget` (view, keys/mouse/wheel/focus), `IRmtHost` (timers, message boxes, cursors, key state, title/status bar) |
| `qt/RmtQtKeys.cpp` | key events → Win32 VK codes: on Linux by physical key (scan code), like a US keyboard on Windows |
| `qt/RmtQtDialogs.cpp` | the dialogs of the tracker, in Qt: `CDialog::DoModal()` → `IRmtHost::DoModal()` → the dialog of `m_nIDTemplate` (`IDD_*`), which reads and writes the dialog's data members; an unknown `IDD` is cancelled |
| `qt/QtMainFrame.cpp` | the `CMainFrame` members the GUI code uses |

### Built-in 6502 and POKEY (`src/emu`)

| File | Replaces | |
|------|----------|---|
| `emu/Cpu6502.cpp` | `sa_c6502.dll` | NMOS 6502, documented and stable undocumented opcodes (LAX, SAX, SLO, RLA, SRE, RRA, DCP, ISC, ANC, ALR, ARR, AXS, SBC $EB, the NOPs), cycle counted (page crossing, branches), flat 64 KB memory; `C6502_JSR` runs until the RTS or the cycles |
| `emu/PokeySound.cpp` | `apokeysnd.dll` | one or two POKEYs (`PutByte` 0x10.. = second chip), cycle based model (clocks, 16 bit, filters, polys, distortions), 44100 Hz, 2 interleaved channels |

Derived from the emulation of `AT2019/ATARI-Driver/RmtSkeleton/tools/rmtplay`.
`C6502.cpp` and `Pokey.cpp` call them directly (the DLLs of RMT are not loaded any more, on Windows too).
Checked with `RmtCoreTest --play` on gemx.rmt against `rmtplay` (an
independent player and 6502 core): over 800 frames AUDC and AUDCTL are
identical and AUDF within 1 (RMT recomputes its frequency tables from the
tuning, rmtplay has the original tables); the sound correlates 0.98 (chroma)
and 0.97 (loudness per frame).

---

## MinGW (engine and backends only)

**What builds with MinGW on Linux (cross-compile):** the RMT engine and the
audio/MIDI backends (`-DRMT_BUILD_CORE_ONLY=ON`). The full tracker needs a Qt6
built for MinGW: MSYS2 has one, it is the GitHub build for Windows (see below).

Prerequisites:
- MinGW-w64 x86_64 compiler (Linux: `sudo apt install cmake mingw-w64`)
- CMake 3.25+

### Cross-compile from Linux

```bash
sudo apt install cmake mingw-w64
cd RITMO-Music-Tracker
cmake -B build-mingw-core -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
      -DRMT_BUILD_CORE_ONLY=ON -DRMT_CORE_TEST=ON
cmake --build build-mingw-core -j
```

Output in `build-mingw-core/out/`:

| File | Content |
|------|---------|
| `RmtCoreTest.exe` | the RMT engine (Song, Tracks, Instruments, C6502, Pokey, IO, SAP/ASM/WAV export) + the GUI-shared drawing code, without Qt |
| `Ritmo.exe` | audio backend test (`main-portaudio.cpp`) |
| `RmtMidiTest.exe` | MIDI backend test (`main-midi.cpp`) |

Notes:

- `mingw-toolchain.cmake` picks `x86_64-w64-mingw32-g++-posix` when present.
  On Debian/Ubuntu the plain `x86_64-w64-mingw32-g++` uses the "win32" thread
  model, which has no `std::thread` / `std::this_thread` (used by the test
  programs).
- PortAudio is optional on Windows: when `portaudio.h` is not found the
  PortAudio backend is left out (`RMT_NO_PORTAUDIO`) and the audio factory uses
  DirectSound. DirectSound and WinMM come with MinGW.
- Without `-DRMT_BUILD_CORE_ONLY=ON` the full build needs Qt6 for MinGW (see below).

### Windows with MinGW native (MSYS2)

Same targets, from an MSYS2 MinGW64 shell:

```bash
cmake -G "MinGW Makefiles" -B build-mingw-core -DRMT_BUILD_CORE_ONLY=ON -DRMT_CORE_TEST=ON
cmake --build build-mingw-core
```

(Not tested on Windows; the sources are the same as the cross-compile.)

### Linux native core build

```bash
cmake -B build-core-linux -DRMT_BUILD_CORE_ONLY=ON -DRMT_CORE_TEST=ON
cmake --build build-core-linux
./build-core-linux/out/RmtCoreTest
```

---

## Code style: clang-format and clang-tidy

The sources use the K&R style: the opening brace goes on its own line only
for function definitions, everywhere else it stays on the line of the
statement, and `} else {` is cuddled. Indent is 4 spaces, no tabs. The rules
are in `.clang-format` and `.clang-tidy` at the top of the repository; any
clang-format / clang-tidy 11 or later reads them (on Debian/Ubuntu:
`apt install clang-format clang-tidy`).

Left out on purpose: the third-party and generated sources (`src/lzss_sap.*`,
`src/resource.h`) and `legacy/`. The message maps and
hand-aligned tables sit between `// clang-format off` and
`// clang-format on`; do the same for new tables of that kind.

### Format

```bash
# One file, in place
clang-format -i src/Song.cpp

# Everything (the same file list as above)
git ls-files 'src/*.cpp' 'src/*.h' 'src/*.c' \
  | grep -vE '^src/(asap\.[ch]|astil\.[ch]|info_dlg\.[ch]|wasap\.[ch]|lzss_sap\.(cpp|h)|resource\.h)$' \
  | xargs clang-format -i

# Check only: prints the differences and fails, changes nothing
clang-format --dry-run -Werror src/Song.cpp
```

clang-format 11 sometimes needs a second pass to settle; run it again until
`--dry-run -Werror` is quiet.

### Check

clang-tidy needs the compile database, which CMake writes with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`:

```bash
cmake -B build-qt -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-qt

# All the sources of that build
run-clang-tidy -p build-qt -quiet '/src/'

# One file
clang-tidy -p build-qt src/Song.cpp
```

It checks `readability-misleading-indentation` and
`bugprone-suspicious-semicolon`, i.e. that the layout matches the control
flow. clang-tidy 11 reports `clang-diagnostic-error` in
`src/qt/RmtQtDialogs.cpp` (lambdas capturing structured bindings, which GCC
accepts and clang supports from version 16): that is not a style problem.

---

## Versioning

The version string baked into the binary is derived **at CMake configure time**
from the git tag of the commit, or from `RMT_BASE_VERSION`. A single source of truth; no hardcoded version
strings in C++ source files.

| Situation | Version shown |
|-----------|--------------|
| On an exact tag `v2.3-rc1` | `2.3-rc1` |
| Any other commit | `RMT_BASE_VERSION`, e.g. `2.5` |

`RMT_BASE_VERSION` in `CMakeLists.txt` is the numeric `MAJOR.MINOR` used by
the release scripts. The generated header `RmtVersion.h` (build directory)
exposes `RMT_VERSION_FULL` and `RMT_VERSION_STRING`.

### Release candidate workflow

Each push to the main development branch should be tagged as a release
candidate. Use `scripts/push.sh` instead of a bare `git push`:

```bash
./scripts/push.sh            # auto-creates v2.1-rcN (N = last+1) and pushes branch + tag
```

The script reads `RMT_BASE_VERSION` from `CMakeLists.txt`, finds the highest
existing `v<BASE>-rcN`, increments N, creates the tag, and pushes both the
branch and the tag to `origin`.

### Final release

When the release candidate cycle is finished, write the release notes in
`doc/release-notes/v<version>.md` and commit them: the first line is
`# <release title>`, the rest is the body of the GitHub release (see
`doc/release-notes/v2.2.md`). The CI builds only attach the packages, they do
not write title or notes. Then:

```bash
./scripts/release.sh 2.3     # creates v2.3, pushes branch + tag, sets title and notes
```

The script refuses to run when the notes file is missing or not committed.

After a release, update `RMT_BASE_VERSION` in `CMakeLists.txt` to the next
development target (e.g. `"2.2"`) so subsequent RC tags follow the new series.
The last release is `v2.4`; `RMT_BASE_VERSION` is `"2.5"`, the version in development.
A commit without a tag reports exactly that, `2.5`; a tagged one reports its tag.

---

## CMake Options

- `-DRMT_QT_MAJOR=6|5` - Qt version of the frontend (default: Qt6, else Qt 5.15)
- `-DRMT_BUILD_CORE_ONLY=ON` - Build the engine and backends only, no Qt
- `-DRMT_CORE_TEST=ON` - Also build `RmtCoreTest` (with `RMT_BUILD_CORE_ONLY`)
- `-DCMAKE_BUILD_TYPE=Release` - Build optimized release version
- `-DCMAKE_BUILD_TYPE=Debug` - Build with debug symbols
- `-DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake` - Use MinGW toolchain (for cross-compile)

---

## Troubleshooting

### Qt6 not found
Install the Qt6 development package: `sudo apt install qt6-base-dev`, or point
CMake at another Qt6 with `-DCMAKE_PREFIX_PATH=/path/to/Qt6`.

### No sound with the Qt frontend
PortAudio was not found at configure time: `sudo apt install portaudio19-dev`,
then configure again.

### DirectSound/WinMM not found (cross-compile)
This is expected on Linux. The libraries are found in the MinGW sysroot:
- `/usr/x86_64-w64-mingw32/lib/libdsound.a`
- `/usr/x86_64-w64-mingw32/lib/libwinmm.a`

### MinGW not in PATH
Install MinGW: `sudo apt install mingw-w64`

### CMake configuration fails
Check that all prerequisites are installed and in PATH.

---

## Output Artifacts

- **Linux / POSIX (Qt6, official):** `build-qt/out/ritmo`, with `resources/` next to
  it
- **Windows (MSYS2 MinGW64, Qt6):** `build/out/Ritmo.exe`
- **MinGW:** `build-mingw-core/out/RmtCoreTest.exe`, `Ritmo.exe` (audio test),
  `RmtMidiTest.exe` - engine and backends only, no tracker GUI (see above)

All output binaries are in `out/` subdirectory of the build folder.

---

## Linux, Windows and macOS builds on GitHub Actions

Pushing a tag `v*` (`scripts/push.sh` → `v2.1-rcN`, `scripts/release.sh` →
`v2.1`...) builds the Qt6 frontend on GitHub and publishes the packages as
assets of the release of that tag (a pre-release for the `-rc` tags); every
job also uploads its package and an offscreen screenshot of gemx.rmt (the
smoke test) as a workflow artifact. They can also be started by hand, to
test them without a tag (`gh workflow run build-windows.yml`): then there
are only the artifacts, no release:

| Workflow | Runner | Toolchain and libraries | Package |
|----------|--------|-------------------------|---------|
| `.github/workflows/build-linux.yml` | `ubuntu-latest`, container `ubuntu:20.04` (glibc 2.31, as Debian 11) | GCC 10, `portaudio19-dev`, `librtmidi-dev` (ALSA) of Ubuntu 20.04, the official Qt 6.8.3 (`aqtinstall` in a Python 3.9 venv; Ubuntu 20.04 has no Qt6) | `Ritmo-Linux-x86_64.AppImage` (`linuxdeploy` + Qt plugin): runs on Debian 11 / Ubuntu 20.04 and newer, the build checks that no file in it needs a glibc after 2.31 |
| `.github/workflows/build-windows.yml` | `windows-2022` | MSYS2 MINGW64: GCC, `qt6-base`, `portaudio`, `rtmidi` (WinMM) | `Ritmo-Windows-x64.zip`: `Ritmo.exe`, `resources/`, the Qt and MinGW DLLs (`windeployqt` + `ldd`); `Ritmo-Windows-x64-Setup.exe`: the installer of the same folder, made by Inno Setup from `scripts/ritmo.iss` |
| `.github/workflows/build-macos.yml` | `macos-14` (Apple Silicon) | Apple Clang, the official Qt 6.8.3 `clang_64` (universal) for both. arm64: Homebrew `portaudio`, `rtmidi` (CoreMIDI). x86_64: PortAudio 19.7.0 and RtMidi 6.0.0 built from source for x86_64 (Homebrew no longer installs on Intel). Minimum macOS 12, as Qt 6.8 | `RMT-macOS-arm64.dmg` (native) and `RMT-macOS-x86_64.dmg` (Intel Macs, or Apple Silicon through Rosetta 2): `Ritmo.app` with the Qt frameworks and the libraries inside (`macdeployqt`, checked with the load commands of `otool -l`) |

`Ritmo.app` needs nothing installed. It is signed ad hoc, not notarized, so
macOS asks to open it with right click → Open the first time. Its
`resources/` are in `Contents/MacOS`, next to the program (as in `out/`);
the configuration is in the user's preferences, so a new DMG keeps it.

The Linux steps are `scripts/build-appimage.sh`, which also builds the
AppImage locally in the same system:

```bash
docker run --rm -v "$PWD":/src -w /src ubuntu:20.04 scripts/build-appimage.sh
# build-appimage/Ritmo-Linux-x86_64.AppImage
```

On the runners there is no sound card: the smoke tests print "cannot open the
audio output (PortAudio)" and go on silent, as RMT does anywhere without one.
The Windows screenshot has no menu texts: the `offscreen` platform finds no
fonts there (the tracker draws with its own bitmap font); the real `windows`
platform has them.

PortAudio and RtMidi are used on every platform, Windows too (where RtMidi
uses WinMM).

## Remaining work on the Qt frontend

The Qt6 frontend is the official build on Linux/POSIX and the development
track for the 2.x series. Remaining work:

1. **Windows and macOS** — run the GitHub builds (first tag push) and fix
   what they find; then test the packages on real machines (sound, MIDI),
   and fix what they show

---

## Test Results

Linux native GCC 10: the Qt frontend builds without warnings, shows the main
screen with a song, keys move the cursor, all 81 menu actions are verified via
`RMT_QT_MENU_TEST` (checked offscreen with `RMT_QT_GRAB` / `RMT_QT_KEYS`).
`-DRMT_BUILD_CORE_ONLY=ON -DRMT_CORE_TEST=ON` builds without warnings and
`RmtCoreTest` runs ("finished successfully", `--screenshot` draws gemx.rmt).

MinGW-w64 GCC 10 (posix threads), cross-compiled on Linux, CMake 3.27:

| Configuration | Result |
|---------------|--------|
| `-DRMT_BUILD_CORE_ONLY=ON -DRMT_CORE_TEST=ON` | ✅ `RmtCoreTest.exe`, `Ritmo.exe`, `RmtMidiTest.exe` build (not run: needs Windows or Wine) |
| default (full Qt GUI) | ❌ no Qt6 for MinGW installed (the MSYS2 build on GitHub has it) |

