************************************************************************

RITMO (RASTER Music Tracker - RMT)
==================================

- https://github.com/raster-atari-org/RASTER-Music-Tracker
- Radek Sterba, Raster/C.P.U. (2002-2009), R.I.P.
- Vin Samuel, VinsCool (2021-2024)
- Peter Dell, JAC! (2024-2026).

************************************************************************

Changes in RMT 2.00 (Planned)
-----------------------------
- New extended file format
- New version of RMTPL107.XEX => Where is the source code?
- Include the export settings in the file format instead of repeating the dialogs for user input on every export.
- Always export in all formats (RMT, stripped RMT, XEX, LZSS, VU-Player...), which were set to "active" in the song settings, with one key stroke without further user input at that point. Because the LZSS compression needs to be done only once in this case, saving in all formats comes at practically no cost. Exported files will be placed in a folder named ".exports" and will be named in the format "-VU-Player_V1.xex".

Technical (Linux/POSIX Qt frontend, Qt5 until 2.0, Qt6 from 2.1):
- 2.5: the size of the audio buffer (the pieces PortAudio takes at each callback) is an option of the configuration,
  `AUDIO_BUFFER_MS` in ritmo.ini: 5 to 40 ms in the dialog, 20 ms by default on Linux (PipeWire and PulseAudio work in
  quanta of about 21 ms and crackled with the 5 ms callbacks through ALSA), 5 ms on Windows and macOS. `RMT_AUDIO_BUFFER_MS`
  overrides it for one session, up to 50 ms (beyond the 60 ms of latency of PokeyRenderer.cpp the ring would run dry). The
  "Don't use hardware soundbuffer" checkbox is gone from the dialog: it only changed a flag of DirectSound.
- 2.4: only what changes on the screen is drawn. `CRmtView::DrawAll()` still describes the whole screen every frame, but
  `CDC::BeginFrame()`/`EndFrame()` record the drawing calls and keep a hash per 32x16 tile of the calls that draw into it: a
  tile with the hash of the previous frame is left as it is, and the Qt frontend repaints (and sends to the display) only the
  changed tiles. When the song is stopped, silent and without input for a second, the screen is drawn 10 times a second
  instead of 60. On a Celeron N3060 the program takes 32 % of a core instead of 45 % while playing (plus much less work for
  the X server), 14 % instead of 45 % when stopped.
- The window adapts to the screen: it opens at 1366x768 (the screen of a small laptop, where the whole stereo song screen fits), reduced to
  the screen when that is smaller (a mono song is shown whole with the POKEY registers; a stereo one needs a window of about 1720 pixels
  for the registers); the first start chooses the interface size (100, 200 or 300 %) from the size of the screen; the size and the
  position of the window are kept for the next start; `--scale=N` (100-300) sets the interface size of one session without changing the settings.
- The MFC build (Windows, MSVC) is removed: RITMO is Qt only. Gone are the MFC windows
  and dialogs (Rmt.cpp, MainFrm.cpp, the *dlg.cpp files), the WASAP test cluster, the Visual
  Studio project and solution and the `RMT_USE_QT` option; the compatibility layer is now
  CompatTypes.h / CompatAudio.cpp / CompatDialogs.cpp (was Mfc*).
- The DLLs of RMT (`sa_c6502.dll`, `apokeysnd.dll`, `sa_pokey.dll`) are not loaded any more, on Windows too: the 6502 and the
  POKEY are always the built-in emulations (`src/emu`), and the `sa_pokey.dll` sound driver is gone. The WAV files the program exports
  are byte for byte the same as before.
- The program is renamed RITMO ("RITMO Is a Tracker for Music On Atari"): the executable
  is `ritmo` (`Ritmo.exe` on Windows), the packages are `Ritmo-*`, the configuration is
  `ritmo.ini` / `ritmo.conf` (registry key, plist). The settings of RMT (`rmt.conf`,
  `rmt.ini` next to the program) are taken over when there are none of RITMO yet.
  The `.rmt` / `.rmw` files, the RMT drivers and the `RMT_*` environment variables
  keep their names.
- Built-in 6502: the stable undocumented opcodes (LAX, SAX, SLO, RLA, SRE, RRA, DCP, ISC,
  ANC, ALR, ARR, AXS, SBC $EB, the multi-byte NOPs) for the RMT drivers that use them.
- Scripting from the command line, as in the Windows program and the Java port of
  RMT 1.36: `ritmo /SCRIPT:<file>` runs a script (open, save, export in the eight
  formats with the dialogs' options, set ntsc/driver/overwrite/output, midi,
  dump actions, dump notekeys, echo) without showing the window and exits with its code;
  see doc/rmt_scripting.md and scripts/test-scripting.sh. The export dialogs
  were split into the dialog and a dialog-independent "Apply" half
  (ExportAsAsmApply, ExportAsRelocatableAsmForRmtPlayerApply,
  ExportAsStrippedRMTApply, CSAPFile::ParseSubsongs, CSongExporter::DefaultXexText/SetXexText),
  the message boxes of a script go to the console (ScriptMessages.cpp).
- The Windows standard keys of RMT 1.36: New is Ctrl+N (was Ctrl+W), Open is Ctrl+O (was Ctrl+L, the item
  is "Open..." now), Save As is Ctrl+Shift+S, Help is F1. The three song functions that used Ctrl+N/O/P
  moved: "Insert new empty unused track" to Ctrl+T, "Insert copy or clone of song lines" to Ctrl+K, "Insert
  new line with unused empty tracks" to Ctrl+J. The editing parts are F2 (Tracks), F3 (Instruments),
  F4 (Song) and Shift+F4 (Info), as in 1.36; F1 was Tracks, F2 Instruments, F3 Info.
- File menu of the Qt frontend: Print... (Ctrl+P), Print Preview and Print Setup... print the tracker screen on a
  landscape page, as the Windows program prints its view (QPrinter, Qt PrintSupport); Properties (Alt+Enter) shows
  the file, format, name, channels, video standard, speeds, maximal track length, the instruments and tracks used
  and the size of the RMT module. (RMT_QT_PRINT_PDF=<file> prints into a PDF file without dialogs, for tests.)
- Fixed: the SAP type B export (File > Export As > SAP) could not be played by any player. It saved the memory
  blocks of the first VU-Player ($1900-$27FF), but VU-Player V2 lives at $0C1B-$1F3F, so INIT jumped into empty
  memory ("INIT routine didn't return" in ASAP), and its song index had eight bytes written to one address.
  It now lays the subtunes out as the XEX export does (CSongExporter::BuildLzssSubtunes), saves the player and the
  index and streams, and a "Subsongs" line such as "00 10" makes one subsong per songline (it was only counted).
  Checked with ASAP: 30 s and more of the song, two subsongs differ, the sound follows ASAP's own RMT player.
- The menu of RMT 1.35/1.36: File, Edit, View, Play, Channels, Song, Instrument, Track, Block, Pokey, Tools and Help
  (Reload is Reopen, the Export item has no dots, Edit Tracks/Instruments/Info/Song and Switch Edit Mode are in Edit,
  Play has the play commands, Channels the channels 1-8 and mute, unmute and solo, Pokey has the registers view and the
  explorer mode, Tools has Options, Tuning, Open ASMA, Open ASAP File (grey) and Run Script). The menu and the keys
  are defined in src/Rmt.rc only: cmake/GenerateRcTables.cmake turns its IDR_MAINFRAME MENU and IDR_MAINFRAME
  ACCELERATORS into tables (RmtMenus.h) from which the Qt frontend builds its menu bar and its shortcuts, so the .rc is
  the one place to change a menu or a key. The shortcuts belong to the main window (the dialogs on top of it keep their own
  keys). New commands: Channels 1-8 (Ctrl+1-8, checked while they play), Mute/Unmute Active Channel (F9), All Channels
  (Shift+F9), Solo (Ctrl+F9), Set and Clear Bookmark (F8, Ctrl+F8), Toggle NTSC/PAL (Ctrl+F12), Activate Pokey
  Explorer Mode (Ctrl+Shift+F5), Increase and Decrease Step Size (Num + / Num -; the volume of the new notes moves to Ctrl+Num +/- and
  Shift+Num +/-, as in 1.36), Open ASMA, Run Script. Esc, Ctrl+Space and the keypad's + and - stay keys of the editing code
  (they go on to the part being edited). Ctrl+S asks
  whether to overwrite also from the menu and the toolbar now (the option "Prompt a save dialog each time CTRL+S is
  pressed" was only honoured by the key handler, which the menu shortcut bypassed).
- Removed the parts of the "RMF" file format that Raster started but never completed (MakeRMFModule, InstrToAtaRMF,
  TrackToAtaRMF), as in 1.35. The MFC build (Windows) runs scripts too: `Rmt.exe /SCRIPT:<file>` (the former /TEST switch
  and the hard-coded developer routine behind /SCRIPT are gone).
- Fixed: in stereo, the left POKEY got the AUDCTL of the right one and the right POKEY an AUDCTL that depended on the
  memory beside the channel flags (CXPokey::CopyAtariMemoryToPokey wrote register 8 twice and read the flag of a
  ninth channel). Each POKEY now gets its own AUDCTL ($D208 and $D218 into registers 8 and $18). Songs whose two
  POKEYs use different AUDCTL bits sound as the Atari plays them; the others are unchanged.
- Fixed: the WAV export of a song at instrument speed 2, 3 or 4 lasted that many times as long (and sounded that
  many times slower, 1-2 octaves lower): the register stream holds one frame per call of the tracker driver, and every
  frame was rendered as a whole VBI. Each frame now renders 1/instrument speed of a VBI (CXPokey::RenderSoundV2Call; the
  remainder of the division is carried, so the WAV lasts as long as the stream). The stereo reference song, at instrument
  speed 4, lasted 486 s instead of 121.6 s. Songs at instrument speed 1 are unchanged. The same defect is in the Windows
  program. Found by rendering the SAP-R stream through ASAP's POKEY and comparing it with the WAV
  (scripts/check-wav.py is part of scripts/test-scripting.sh).
- SAP type B and XEX exports of stereo songs work (the stereo flag, the second POKEY and the 18-byte frames) as long as the compressed
  data fits the memory of the LZSS player ($2040-$BFFF, 49,088 bytes); checked with ASAP on the first songlines of the stereo
  reference song (the L and R channels match ASAP's own RMT player at 0.86, crossed 0.72). The whole reference song needs 69,782
  bytes - 121 s of eight channels at instrument speed 4 - and is refused with a message that names both sizes; the
  register dump itself is 437 KB, so only a longer-range storage than the 64 KB of the Atari (or the RMT player itself in the
  SAP, which this program does not export) could hold it.
- Ported from the 1.36 development of the Windows/Java repository: QWERTZ keyboard
  layout (the first start takes the layout from the keyboard language), the Pokey
  Explorer in CPokeyController with the Pokey menu and positional keys, and bug fixes
  (CFraction::operator==, DEFSONG line of the SAP files, RMW main parameters of 4
  bytes, heap buffers for the LZSS exports, MIDI note off quantization, zeroed
  instruments and POKEY renderer members, the dump of the registers leaves the
  cursor and the play time unchanged).
- The Qt frontend moved to Qt6 (6.8.3 in the packages of 2.1): CMake takes
  Qt6, or Qt 5.15 where Qt6 is not installed (-DRMT_QT_MAJOR=5|6 chooses).
  The GitHub builds use Qt6 on Linux (official Qt, aqtinstall), Windows
  (MSYS2) and macOS (official universal Qt); macOS 12 is now the minimum.
  (2026-09-29)
- Full menu bar added to the Qt5 frontend (branch feature/Qt-Side, 2026-09-28).
  All 7 top-level menus (File, Edit, Track, Block, Instrument, Song, View, Help)
  with nested submenus and keyboard shortcuts are built from the MFC
  IDR_MAINFRAME MENU resource structure. Menu item enabled/checked state is
  updated automatically via ON_UPDATE_COMMAND_UI handlers (QtCCmdUI bridge).
  All 81 leaf actions have registered handlers; verified by the RMT_QT_MENU_TEST
  headless test hook (QT_QPA_PLATFORM=offscreen, exit code 0).
  Help commands (ID_HELP_HELP_TOPICS, ID_HELP_ONLINE_HELP, ID_HELP_ABOUT_APP)
  are handled directly in the Qt Dispatch() layer since CRmtApp is MFC-only.
- Version string unified: derived at CMake configure time from the nearest git
  tag (v2.0-rc1 → "2.0-rc1", between tags → "2.0-rc1+dev", no tag → "2.0-dev").
  Single source of truth via src/RmtVersion.h.in / generated RmtVersion.h;
  no hardcoded version strings in C++ source. (2026-09-28)
- Release scripts added: scripts/push.sh auto-increments v2.0-rcN and pushes
  branch + tag; scripts/release.sh creates a plain v2.X release tag. (2026-09-28)
- Fixed the Qt frontend using 100% of all CPU cores. (2026-09-28)
  The song timer (CompatAudio.cpp, timeSetEvent) re-created from its own tick
  could start its callback while the previous one was still running, whenever
  the timer fell behind by more than one tick: CSongTimer then kept two live
  timers, their number doubled at every stall, and CSong::TimerRoutine() ran
  concurrently (corrupted 6502 state, thousands of threads, abort). A
  re-created timer now waits for the tick that created it, and a timer more
  than 200 ms late restarts from now instead of catching up.
- Fixed a segfault on exit of the Qt frontend: the song timer kept ticking while
  g_Song was destroyed. CSongTimer::StopTimer() now runs on every exit, and a
  lock plus a "stopped" flag keep a running tick from creating a new timer.
- Qt frontend redraw made cheaper (idle CPU about 50% of one core, was 78%):
  the widget shows the view's bitmap directly (no full-screen copy per frame,
  scaling done by QPainter), and CDC::BitBlt / StretchBlt of the non-MFC
  builds clip once and copy whole rows. The 60 fps screen refresh and the
  50/60 Hz song timer (VBI) are unchanged and independent of each other.
- All compiler warnings fixed: the Qt and core-only builds (GCC 10, -Wall
  -Wextra) went from 191 warnings to none. (2026-09-29)
  Real bugs found among them:
  - Undo: event data was freed with delete[] on a void*; it is now freed with
    the type it was allocated with.
  - CInstruments / CTracks: the constructors tested m_instr / m_track before
    they were initialised, and the destructors used delete instead of delete[].
  - Loading Atari binaries: `!sizeRead == size` never detected a short read,
    now `sizeRead != size`.
  - POKEY registers view: a 2-byte buffer received the register number via
    sprintf (overflow); several values could be read uninitialised.
  - Tuning file: a ratio line without '/' used an uninitialised denominator,
    it now defaults to 1.
  - MOD import: the envelope length of looped instruments was computed but
    never assigned (statement with no effect); it is now set.
  - TMC import: the volume fade-out was clamped to 0..255 after the conversion
    to BYTE, so the clamp did nothing; it is now clamped before.
  Cleanups: missing default cases in switch statements, [[fallthrough]] on
  intended fall-throughs, C++20 volatile deprecations, strncpy replaced by
  memcpy for unterminated copies, unused variables, a nested comment, and the
  empty src/import.cpp removed.
- The Qt5 frontend is now the official build on Linux/POSIX and was merged into
  master. BUILD.md starts with it, README.md has a "Building" section. Windows
  keeps the MFC GUI built with MSVC by default (RMT_USE_QT=OFF) until the Qt
  frontend is tested there. (2026-09-29)
- Qt frontend: the file dialogs work (QFileDialog). Songs load and save
  (RMT, TXT, RMW), instruments (RTI) and tracks (TXT) load and save, and
  Import / Export As show their file choice. CFileDialog::DoModal() asks the
  frontend via IRmtHost::FileDialog, so IO_Song.cpp is unchanged; the chosen
  file type, last folder and proposed file name behave as with MFC, and upper
  case extensions are matched too. (2026-09-29)
- Fixed loading TXT songs and instruments with LF line ends (files written on
  Linux): an empty line made the reader swallow the next "[SECTION]" line, so
  only the [MODULE] header was loaded. Files with CR LF line ends were not
  affected. (2026-09-29)
- Qt test hooks RMT_QT_COMMANDS (trigger menu commands) and
  RMT_QT_FILEDIALOG (answer file dialogs) added. (2026-09-29)
- Qt frontend: File -> New works (dialog "New RMT module": track length and
  mono/stereo, confirmation above 64 lines). CDialog::DoModal() now asks the
  frontend via IRmtHost::DoModal, which picks the Qt dialog by its IDD
  (src/qt/RmtQtDialogs.cpp); the other dialogs are still cancelled. Test
  hook RMT_QT_DIALOG shows, saves and confirms dialogs in test runs.
  (2026-09-29)
- Qt frontend: File -> Import works. The ProTracker (MOD) and Theta Music
  Composer (TMC) import options and the "Import of module finished" dialogs
  are rewritten in Qt with the MFC defaults and behaviour (all options on,
  Fourier disabled, "I understand" needed for OK and remembered). Checked
  with the samples rmt/imports/axel_f.mod (MOD) and 404_Error.tmc (TMC).
  (2026-09-29)
- Qt frontend: File -> Export As works for all formats but WAV. The options
  of stripped RMT (address, SFX, RMTFEAT definitions), ASM simple notation,
  relocatable ASM for RmtPlayer, SAP-R / SAP and XEX (screen text preview,
  rasterbar color) are rewritten in Qt with the behaviour of exportdlgs.cpp.
  LZSS has no options, which is why it was the only format that worked
  before. The WAV export still fails with "Could not get sound format!".
  (2026-09-29)
- Qt frontend: View -> Configuration works: the "RMT configuration" dialog
  (general, keyboard and MIDI options, driver version) and its "Paths..."
  dialog are rewritten in Qt with the MFC defaults and checks. (2026-09-29)
- WAV export fixed (it always failed outside Windows, and wrote no sound
  with apokeysnd.dll or the built-in POKEY). (2026-09-29)
  - The non-MFC build writes the WAV file with std::ofstream; the mmio*()
    API it used is Windows only (CompatTypes.h had always-failing stubs).
  - RenderSoundV2() only rendered with sa_pokey.dll; it now renders with the
    APOKEYSND driver too (apokeysnd.dll and the built-in POKEY).
  - Stereo songs: the 2nd POKEY is the first half of a stereo stream frame;
    the export read it as the 1st POKEY, so the left channels were lost.
  - The timer routine kept playing and rendering on its own thread during
    the WAV rendering (same Atari memory and POKEY emulation); it is now
    bypassed as during the stream dump (CSong::SetStreamRendering()).
  - After an export through the POKEY stream (LZSS, SAP-R, SAP, WAV) all
    channels stayed muted: CSongContainer now ends the dump with
    FinishedRecording(), which switches them back on.
- Fixed the stripped RMT export writing one byte more than the module (the
  end address given to SaveBinaryBlock, which is inclusive, was the first
  byte after the module), and writing an empty file for a module ending at
  $FFFF (that end address wrapped to 0). (2026-09-29)


Changes in RMT 1.35 (Planned)
-----------------------------
- Include the instruments and samples in the download again.
- Remove the parts of the "RMF" file format that Raster started but never completed.
- Have an additional ".ini" file as an intermediate step to the RMT file format version 2. There, the module-specific settings from the "RMT.ini" and "Tuning.ini" could be preserved. Also, the file's existence indicates it is an RMT in 1.34 format.
- Map the official Windows standard key combinations to the correct Windows standard function. Find alterative for their current binding.
  - Help Topics (n/a) => Help Topics (F1)
  - New (Ctrl-W) => New (Ctrl-N)
  - Load (Ctrl-L) => Open (Ctrl-O)
  - Print =>Print (Ctrl-P)

Technical:
- Extract the binaries for the tracker drivers from the source code and have them as resources in the file system. This way, it is easier to inspect and update their content.

Changes in RMT 1.35
-------------------
- A complete version history was added (doc/rmt_version.md), including external download links and (where possible) branches. (2026-01-12)
- The GitHub repository for RMT 1.34 at "https://github.com/VinsCool/RASTER-Music-Tracker" was archived by VinsCool. It will be kept as a reference and still includes several features (e.g., keyboard layout handling) that might find their way into RMT 2.0. (2026-01-08))
- Technical documentation for the RMT tracker and the RMT current and future module file format versions was created. (Build 2026-01-06)
Technical:

- The debug display was extended to include the character of the last pressed key and the SHIFT and CTRL modifier keys (raster-atari-org#9). (Build 2026-01-12)
- The following parts were changed (Build 2026-01-06)
  - The initialization during the start.
  - The loading/handling of the CPU/POKEY emulation is now prepared to do both later with ASAP.
  - The drawing of the tracker screen.
  - The logic for loading, saving, and editing tunings (tuning dialog window).
- Most remaining untranslated parts of the documentation and resource files were translated/changed to English. (Build 2026-01-06)
- The repository's file system structure was reorganized and cleaned up. (Build 2026-01-06)
- The solution was updated to Visual Studio 2026, toolset v145, only 64-bit builds from now on (Build 2026-01-06)

Changes in RMT 1.34.00 
----------------------
- RASTER MUSIC TRACKER is now Open Source! Thanks to everyone who made this possible!
- Rewrote all of the RMT Export code, for proper SAP-R and LZSS export formats support
- Fixed a lot of bugs missed in the previous release
- Cleaned up all the code in a really aggressive way, making things much easier to follow without commented-out blocks everywhere.
- Finished translating ALL of the RMT code, so now all comments/reference are available in English.
- Re-structured the entire codebase as a side effect of the sources becoming available to the public.
- Removed most of the unused code unless it may have use in the future.
- VinsCool is now clinically insane (just kidding!).
- This release was a LOT of work, you have no idea how many nights I spent sleep deprived for making it as good as it could be!


Changes in RMT 1.33.00 BETA
---------------------------
- THIS VERSION IS A MESS, AND AS SUCH, DOES NOT REFLECT THE GOALS THAT WERE SET FOR RELEASE!
- Add full Tuning computation code, borrowed from the [POKEY Frequencies Calculator](https://github.com/VinsCool/POKEY-Frequencies-Calculator) by VinsCool.
- Early SAP-R Dump and LZSS export capabilities, hardcoded and junk implementation, good as a proof of concept ONLY.
- Several changes in the sa_pokey procedure code, intended for the Altirra POKEY emulation plugin support improvements.
- Various bugfixes and experimental changes not reflected in the changelog, but generally made RMT more stable.


Changes in RMT 1.32.07
----------------------
- Cleanups, and lots of under the hood work for the future releases


Changes in RMT 1.32.06
----------------------
- Added a new tracker.obx function, where the driver version is written in plaintext, and then displayed in the About dialog. This is purely cosmetic, otherwise, there is no difference.
- Make the XEX export format have the color shuffling optional.
- Also make sure the SAP exports skip the color shuffling instructions, since both export formats use the same player binaries.
- Added an option for XEX exports to enable or disable the automatic region detection, in case a particular setup may work incorrectly with it.
- Fixed a bug related to scaling, causing the output to have choppy appearance due to incorrectly having set the boundaries of the display to draw every frame. Now it's a smooth display!
- Improved the PAL/NTSC detection code in the exports player (thanks pps for the sample code I used as a reference!).
- Updated all binaries with the last 6502 ASM changes, the old export binaries will no longer be compatible due to several memory address changes!


Changes in RMT 1.32.05
----------------------
- Added scaling support. 100% to 300% There is no filtering yet, only integer multiple of 100% will look pixel perfect.


Changes in RMT 1.32.04
----------------------
- Re-introduced the Distortion 6 BASS16 code in RMT, with updated description. You can also set the Distortion used for it with CMD6. $0Y = Distortion
- Small update on Distortions and Commands description. Added infos for new BASS16 CMD6 code, as well as Distortion A Sawtooth mode using AUTOFILTER + CH1+3 1.79mhz mode.


Changes in RMT 1.32.03
----------------------
- Tweaked the "Go to line: XX" appearance by moving it a few tiles to the left, in order to mask the line number. This may help reduce the number accidental recursive GOTOs when they are edited
- Also changed the GOTO colors so only the number color will be changed when highlighted. The number will be white by default
- Added a line separating the Left/Right tracks in the SONG block, making it easier to identify which channels belong to which side
- Small improvement to make the "Go to line" on tracks more appealing visually: highlight the number when the cursor is actually active in tracks, and keep the "Go to line" text more visible as well.
- Set the cursor position on the Volume envelope by default for instruments, to avoid accidentally overwriting the name when cycling through instruments (thanks PG for the suggestion!)
- Properly fixed the Follow Cursor Play bug by also making sure the parameter was defined in the Subsong change as well
- Fixed a bug where using the PAGEUP/PAGEDOWN keys in tracks at the same time as play+follow mode is active would cause graphical glitches since the line would try to move at the same time of following the cursor


Changes in RMT 1.32.02
----------------------
- Fixed a bug when playing a song with follow cursor enabled would always switch back to "play from current position" mode even if the mode was set to "track loop"
- Fixed a memory leak caused by the 16-bit frequencies calculations, which lead to a chance out of 2 to cause a crash when RMT is being executed
- Changed the empty tracks appearance from a single dash per row to a dotted pattern identical to tracks with data into them (thanks zaxolotl for the suggestion!)


Changes in RMT 1.32.01
----------------------
- Added 16-bit pitch accuracy frequencies display for Distortion 2, A, C and E (C table 2)
- Added Sawtooth pitch frequencies display, as well as if the waveform is inverted by swapping the CH1 and CH3 values

Changes in RMT 1.32.00
----------------------
- Updated the driver binaries to a newer and better WIP version, including export binaries. Work in progress ASM code is also included. Beware, it is messy...
- Bump the version number to 1.32 since the driver update is a major change, and numerous fixes will be done since the last 1.31 revision
- Clearly show the exact version number, since RMT may be updated pretty often, without really being warrant of a major release... Like this one for example :P


Changes in RMT 1.31.22
----------------------
- Tweak the NTSC timing hack, should be a little closer to the actual thing now
- Fix the step size of 0 to force a movement of 1 when movement keys are used, to prevent getting stuck (thanks Ravancloak for pointing this out!)
- Force a .ini file write if no config is found when RMT.exe is launched, setting the default configuration (thanks again Ravancloak!)
- Added one of my own tunes I missed last time (Stranded on the Surface of Io Final.rmt), and updated a tune upon author's request (ilusia.rmt)


Changes in RMT 1.31.21 (VinsCool's version, first public release)
----------------------

--- The story begins around the month of June 2021 ---

RMT Patch 16 started simply as my own hacked driver version, and was nothing more than tuning tables experiments, based on the Patch 8 hack by Analmux.
Slowly, it had become its own thing, and I started learning 6502 Assembly during summer.
Months were passing, and I kept pushing my experiments further, to the point I was *literally* reverse-engineering RMT from absolutely nothing for reference!
Then, while I was really close to give up, feeling horribly frustrated and upset.
I was not able to do anything without the experience, or just a tiny bit of the original code to lead me in the right direction, and during the month of October, I really thought I had reached the dead end.

But then, something unexpected happened!
I was contacted via email by Mathy, and was very kindly sent the actual source code of RMT!
I was so shocked, in fact, I almost refused to even touch it, scared I'd destroy the legacy of Raster, and everything he had built the foundations, before he had left this world 10 years ago.

There I was, sitting with my huge messy reverse-engineering efforts, and the actual files I was wishing so much I had the chance to use...
After having exchanged some more emails regarding what I was allowed to do and not to do, I thought I was in good terms with the people keeping the original code safe.
So as far as I understood, as long as I did not distribute the source code to anyone else, I could do pretty much anything I wanted with it. 

So thanks again, Bobik, and Fandal I believe? As well as Mathy for relaying the emails, for granting me the privilege of having the RMT code in my posession!
Once I made sure I had the permission to talk about my experiments, and mention I was actually working from the official source code... I was determined to make good use of it...

So I spent a considerable amount of time learning C++ during the following month, just for the sake of pushing onward, and experiment with the RMT code, in hope I could expand it, and also improve it to my liking.
And all of this was really awesome, and fun!
Anyway, that was the story so far, so here we are at the first wave of changes I did into the RMT code, which was at that moment broken by my fault, mostly because I had *literally* no experience coding in C++ at all!

--- So here we are, around the month of November 2021 ---

Changes below came from the original 1.29 code I was working on.
Things were a lot mixed up, but eventually were merged then forked into its own direction.

At that point, I had simply called this fork "RMT 1.29+Patch16", because of how hacked up and just borderline functional it was during that time.

- Fix the UI bugs I have introduced with my experiments 
- Adjust the UI to either 4 or 8 tracks layout, making things look cleaner in general
- Adjust the Instrument popup window width so full names can be visible, making instruments with long names easier to find
- Fix the exports formats to be compatible to the Patch 16 design. Full support of MONO, STEREO, and NTSC parameters, in both SAP and XEX formats (still in progress for some details, but they are fully functional)
- Re-introduce the external binary loader, using the obx extension. This makes a lot of the things I am doing a lot more modular and easier to maintain since I don't have to hack pointers and memory addresses every time
- Allow modules instrument speed to support up to speed 8 / Maximum of speed 4 in exports still!!
- Fix the Tracker UI's hitbox so the 4th/8th channel's speed column could be clicked using the mouse pointer
- Change ALT+DEL to CTRL+Z for undo, same for the redo whatever ALT+SHIFT+DEL, to CTRL+Y. This now seems to be fully functional, the previous redo combo had random chances of missing due to the implementation of how keys were checked
- CTRL+TAB jumps to the speed columns of the currently edited channel
- Fix the song timer to always reset when the player stops
- Add the ability to either set a song bookmark, or remove it. Shortcut using it would either play from set bookmark, or from cursor position when unset. Bookmarks used to stay for as long as the module was edited
- Cursor follow during play is now toggled with the F12 key, and playing from one of the shortcuts (now F5, F6 and F7) will not overwrite it, but instead make use of either case, making the overall interface less awkward when it's playing
- Reset the entire sound process (including reloading the .dll plugin) with a SHIFT+ESC combo, helps removing stutters or crackling in some cases
- Fix a small visual bug related to the colors displayed, which caused conflicts with certain colors used together, and added a case for either being in JAM or EDIT mode when necessary
- JAM mode is toggled with CTRL+SPACE, the other SPACE shortcuts were removed (since other, better ways to stop the sounds exist anyway)
- Instruments and song information can now all be edited in Jam mode, leaving the Tracks and Song blocks the only ones which will be in live test mode.
- Permanently display what mode is being used (EDIT, JAM (MONO) and JAM (STEREO)), with matching color palettes (still in progress)
- Display Two-Tone Filter, requires the Altirra plugins for proper functionality 
- PAL/NTSC display in the info area, toggle in-tracker using CTRL+F12
- Colors fix in the UI
- New font by PG
- Fix the SONG block size, as well as its clickable area, and the offset related to it so the currently playing line is centered too
- Add a PAL/NTSC toggle from a mouse click in the text area associated to it, behavior is identical to CTRL+F12 when it's executed
- Set MAXTRACKLENGTH and switch between 4 and 8 TRACKS from the main interface.
- New Global UI commands: Song Line Seek Next and Previous. Use with either Multimedia Keys (Seek Next/Prev)
- Tweak the clickable hitboxes slightly when the text was variable width for the MONO/STEREO and PAL/NTSC boundaries

This is around that time, about a month later, that another surprise had happened, spicing up this adventure!
To make a pretty short story, out of nowhere, I was also sent the sources of RMT 1.30 this time! 

This version, which was another fork version, added a couple of new things, but was then left dormant by the original author, probably for the lack of time or interest.
So thanks a lot, sir Rudla, for allowing me to use your own version for my experiments!
A lot of his changes helped me a lot to get unstuck on a bunch of things I was unsure about, so this was a blessing, on the long run!

--- December 2021's progress... ---

- Bump version to 1.31 (still work in progress at that moment), and removed all the "Patch 16" references since this is pretty much the unofficial continuation of RMT now
- Port to Visual Studio 2019
- Fix a bug introduced by 1.30, which caused the lines of selected objects and AUDCTL hooks to be black, since they were reading zeroes instead of the proper RBG values
- Fix the "centered cursor on rows" view being dynamically adjusted using the window size itself
- Add MinMaxInfo calls for the minimal window sizes, independantly from 1.30's code changes since I took a different approach for it
- Display notes as Flats or Sharps (also saved in rmt.ini)
- Add a toggle for notes to display German Notation, basically B notes as H instead, etc (thanks PG for the idea)
- Change the highlight colors to a more uniform display, design is still not definitive
- Replace the text "Flat or Sharp notes" to "accidentals", being the proper musical term. Thanks to Enderdude for the information!
- Edit the configuration dialog window to make options a little less ambiguous, also re-arrange the elements a little bit
- Small code cleanup, now that most things merged recently (1.29, 1.30 and my own experiments) appear to work properly, few 1.30 additions were purposefully omitted for the time being, since a different approach was being taken
- Display the "About" dialog if no .ini file is detected, this is assuming RMT ran for the first time on a new system. It's the small things!
- Tweaked the commandline argument, so attempting to load any file by association that isn't a .rmt module will throw an error message and exit, instead of loading the tracker with nothing at all
- Changed the F7 hotkey to now Play from current position using F7 alone, or from Bookmark only if SHIFT is held and Bookmark is set.
- New global shortcut: CTRL+W to create a new file. 
- Preleminary Multimedia keys support, also added independently from the 1.30 changes
- Fix the Instrument editor screen to allow using SHIFT + Note Key again, and keep SHIFT as SHIFT for uppercase on the Instrument Name and Song Name instead
- Fully removed the necessity to use CAPSLOCK for typing uppercase characters holding SHIFT, this was counter-intuitive and rather unpleasant to have to use it like that
- Tweak the SHIFT key further when testing notes or editing text, so no accidental values are input if certain characters were pressed while SHIFT was held
- Properly implement CAPSLOCK, which will force uppercase and reverse to lowercase if SHIFT is also being held, it won't affect anything else than the text input areas (Song and Instrument names)
- Add another BOOL flag for the Song name area this time since it caused conflicts with the instruments screen when the same one was used for both at the same time
- Remove the old code and references to CAPSLOCK, NUMLOCK and SCROLLLOCK since they are no longer needed
- Fix a bug where holding SHIFT through CAPSLOCK caused instrument name cursor to function backwards, now having CAPSLOCK active won't cause any conflict with the arrow keys while SHIFT is held
- Tiny change to the is_editing_infos boolean, making sure it's actually only setting 1 when the cursor is on the Song Name area, while the Speed values will always return 0
- Fix a design oversight where editing in the "Track lines boundaries go to next/previous song lines" parameter being disabled would not take the pattern size into account and overflow past the "END" rows
- Make sure the selection block cursor won't cross pattern boundaries, fixing both a graphical bug and the awkward block selection behaviour where it would also overflow into the next/previous pattern
- Allow tracks to be navigated with LEFT and RIGHT keys seamlessly, making all Tracks columns (Note, Instrument, Volume and Speed) accessible unconditionally
- Force the Instrument column to insert a note if the note keys are used on empty rows, effectively making it behave the same as the Note column. Will use the original instrument number function otherwise
- Two-Tone Filter can now display its volume output in a unique color, final design to decide later
- Replace the "." character in empty tracks to "-" for making it easier to visually know where the active line is
- Fix the "now playing" yellow color to only display on the proper song line, and not when the same patterns are used elsewhere
- Fix the newer TRACKS UI blocks position
- Tweak the UI elements a bit to allow larger tracks, making Speed columns their own spot and also did preliminary work on implementing more complex functions in tracks
- Add preleminary support for sa_pokey.dll PAL/NTSC toggle, sending the accurate CPU clock values between each cases
- Fix the X coorfinates in TRACKS hitbox, everything else works as expected
- Adjust the minimal window size to accomodate the new TRACKS UI dimensions
- Preliminary new colors and themeing (thanks PG for the help and suggestions!)
- 256 colors bitmap support
- Tweak the DrawAnalyser() function in order to draw the mute/shadow/full volume bars with unique colors matching the background
- Highlight GOTO lines in the SONG block to make them easier to see (thanks PG for the idea!)
- Fix the config dialog update to not reset the sound routine if no region setting was changed, same for the HW/SW Soundbuffer setting. This was an oversight by Vin, oops
- Move the SONG block depending on the window size. This was the last 1.30 feature that was not backported yet since I took a different approach with the offset displacement handler
- Fix a tiny bit the Volume Analyser in the Instrument Editor screen, the volume bars were 3 pixels off relative to the tracks below it
- Add experimental register state visualiser/debug informations display, works for the few elements in place, but a lot of the design has yet made
- Add few more windows size parameters in order to move the SONG block accordingly, and hide the registers view block if it is too large to display properly
- Fix a small bug related to the WINDOW_OFFSET variable which would run into a conflict with another parameter and cause the UI to flicker as a result
- Add rudimentary frequencies calculation for the Register View. Much still missing!
- Create a Distortion C formulae for 15khz, 64khz and 1.79mhz modes. Accurate frequencies computed in real time!
- Create a Distortion 2 formula only for 1.79mhz mode, same as above!
- Worked around all missing modes by going directly into the Pure (Distortion A) frequencies formulae
- Fix a memory leak in the initialisation of the Registers Visualiser causing glitched characters to pop up at random
- Rewrote the Registers Viewer entirely to make it fit inside a single for() loop, since this caused random corruption in the data being read elsewhere despite sharing the exact same variables
- Fix a tiny mistake where not having the POKEY Chip registers option checked in the View menu would not display the Registers on the right, while the POKEY REGISTERS text shown above was still there
- Fix the Registers View initialisation, which potentially leaked memory by writing characters out of bounds
- Add Distortion 2 64khz and 15khz formulae
- Add Distortion 0 and 8 POLY9 formulae for 1.79mhz and 64khz
- Rudimentary cents off calculation. Needs improvements
- Further improvements in the cents calculation. Seems to output acceptable accuracy withing the range of +-1 error compared to the earlier attempt
- Remap the Registers View into a nicer block, still in progress. Small code cleanup in the AUDCTL bits detection
- Hopefully fix the RMW format for loading and saving due to an error in parameters number being incorrect, causing RMT to crash (thanks Zlew for reporting this bug!)
- Add new compiler flags in order to speed up the general performace of the code, few code adjustments done accordingly (thanks Puna for the tips and advices!)
- Notes computation based on cents and base tuning (440hz currently) is now fully functional!
- Fix the Instrument Editor screen where being in the Instrument parameters make SHIFT not work to switch Instrument with LEFT or RIGHT when the cursor is not on the Instrument Name line
- Also fix the Infos area with the same issue
- Make both "is_editing_" BOOLs more robust by making sure any situation would update the flag accordingly, to prevent keeping the wrong flag in memory when the cursor is moved elsewhere
! Work around the TXT format being broken by preventing the user from loading and saving in that format until it is properly fixed at an ulterior time
- Fix hotkey support holding SHIFT for Infos and Instrument areas, making PLUS, MINUS, PAGE UP and PAGE DOWN work again for Volume and Octave edit, finally after being broken for 3000 years
- Fix the SPACE key input when exiting PROVE/JAM mode, which would be input by accident in the instrument editor, when mode switched.
- Also fix a small bug in PROVE mode where pressing SPACE also reduced the current active volume... because of a missing break; statement at the end of its case entry, oops
- Allow jumping to ANY song line during playback, using either the mouse pointer, or any of the known shortcuts to SongUp() or SongDown() (Thanks Zlew for the suggestion)
- PAGE UP and PAGE DOWN keys can be used anywhere, effectively allowing seeking song lines from everywhere during playback! In Tracks: Use CTRL+Up or Down, or CTRL+Page Up or Page Down for the same effect
- Add Paste Merge (CTRL+M) hotkeys in tracks
- Tweak PROVE keys further so navigation is almost identical to EDIT mode control, minus the ability to edit SONG and TRACK lines. Instruments and Infos can still be edited, use SHIFT to test notes there
- Add Transposition shortcuts using CTRL+F1/F2/F3/F4, F1 and F2 transpose down or up 1 semitone, F3 and F4 transpose down or up 1 octave
- Add the ability to use most Tracks edit hotkeys without selecting a block first. When no block is selected, current row is automatically selected then gets the effect processed onto it
- Tweak the paste shortcuts so the data can be pasted anywhere when a Selection Block is still active, avoiding pasting in that block when it was not wanted
- Add CTRL+SHIFT+LEFT/RIGHT/UP/DOWN shortcuts, Left an Right change the instrument, Up and Down change the volume. If a block was selected first, it will be processed whole, else, it will be the active row
- Press Escape key to DESELECT a block
- Add CTRL+NUMKEY_ADD/NUMKEY_SUBTRACT shortcuts to change the tracks Step value
- Step value is now precessed in all situations in tracks, eg: inputting notes, volume, deleting row, selecting block, etc, except for SPEED column (no step when input), or when ENTER key is used (always 1)
- Add CTRL+SHIFT+ENTER hotkey to input a track end line, useful when you literally cannot use the END key (like myself)
- Multiple tweaks and polish on most things, too many detail to remember, or be worth mentionning, mostly fixing some bugs or very small oddities. Input revamp still in the work, so bugs may happen!
- Fix Tracks lines going out of bounds going up on tracks with Wise Loops due to a maths error
- Fix Select All command to actually select all, up to the Track End line, as it should have been since forever. This is directly related to the oversight mentionned above. Manual selection needs fix too
- Fix Tracks lines during Play+Follow mode to prevent any movement, except on the horizontal axis, so moving the cursor left and right still works. Fixes the oversight introduced with my new Song Lines code
- Fix a off by 1 error in the Select All function, where a line would be selected too far outside the boundaries, causing an overflow and possibly memory corruption when copied then pasted elsewhere
- Edit the Song Lines Goto code to make sure it always starts playing on the very first row of a pattern, to avoid skipping lines by "continuing" the playback from the wrong row on a different pattern
- Fix the Info section where changing the active instrument with SHIFT+LEFT/RIGHT would fail due to a missing parameter in the input handler
- Fix a bug in the Instrument editor where the combo CTRL+SHIFT+PLUS/MINUS for editing the volume envelope would be ignored due to a missing parameter
- Fix a bug in the Instrument editor where the combo CTRL+SHIFT+UP/DOWN would be ignored, also due to a missing parameter
- Fix a bug in the Instrument editor where using CTRL+SHIFT+LEFT/RIGHT would wrongfully return an instrument change command, because of a missing condition in the SHIFT key handler
- Fix a bug in the Instrument editor where using SHIFT+INSERT would wrongfully be ignored, also because of a missing parameter (god fucking damnit I keep finding new ones...)
- Tweak Prove keys a bit to prevent accidental inputs with invalid combinations
- Add TAB key support in Song lines, move track right, or left if SHIFT is also held
- FINALLY made sense of all inputs, will be much easier to edit from now on!
- Hopefully fixed the bugged note/instrument combo input from the Instrument column in tracks, where an empty row would force a note, else, change the instrument value
- Fix a crash caused by attempting to display Volume Only mode in the Registers View. Added few tiny changes in order to display "nothing" instead (thanks Enderdude for pointing this out!)
- Tweak global shortcuts further to avoid any conflicts with PROVE keys in stereo (CTRL+SHIFT+NOTEKEY)
- Add HOME and END keys support in PROVE mode, HOME to the first line, and END for the last
- Re-add the SONG Lines shortcuts "Insert New Unused Pattern", "Duplicate Pattern", "Set Goto Line"", now I realise those were nice to have there too
- Tweak Block Selection behaviour, so now manual selection using SHIFT+UP/DOWN in tracks follow the same logic used by CTRL+A, allowing selection of all valid track lines properly
- Tweak TrackUp() and TrackDown(), to make sure "going past boundaries move Song Line to the Next/Previous position" is properly happening only if the configuration is also enabled
- Also Treak the Tracks up/down code further so having a block selection active could never go past the boundaries, and just not move further on the active line, regardless of the boundary setting active
- Make the Home and End keys in tracks have a much smarter behaviour, where holding SHIFT for a Selection Block will match the improved logic done a bit earlier
- Also allow END to either go to the last wise loop line/end line or actual last line, replicating the behaviour of HOME which goes either to the first line in track, or first line in a Wise Loop
- Force the Tracks ENTER key case to always move down, to prevent the Selection Block's last line to get stuck, caused by the improved logic's check for the last line while the block is also active
- Update the manual to match this version's differences and new design
- Correct some English grammar in menus, update strings to better reflect the changes done
- Add experimental NTSC timing, using a pretty cursed setup involving the timeSetEvent() function which gets called every other frame to oscillate the timing between 16 and 17 miliseconds in NTSC mode

--- December 27th, 2021, just barely missed the Christmas release goal :( hahaha :) ---

...This was a MASSIVE changelog, but here we are, finally! 
Enjoy this new unofficial Raster Music Tracker version! More updates and more rambles will come in the near future :D 
~Vin


Changes in RMT 1.30 (Rudla's version)
-------------------------------------

- Main window can be resized
- Fixed bug with uninitialized COM
- Window position is remembered on close, restored on start
- Added recent file list
- Support play/stop multimedia keys
- Config option to disable use of NumLock


Changes in RMT 1.29 (unreleased)
--------------------------------

New songs
- Song "cured.rmt" (by Tatqoo)
  in "songs/tatqoo/" directory.
- Song "h3x0r_menu.rmt" (by Nooly)
  Song "h3x0r_ingame.rmt" (by Nooly)
  in "songs/nooly/" directory.


Changes in RMT 1.28 (last official version by Raster) ... Rest in Peace, good sir, and thanks for everything!)
-----------------------------------------------------

- Recognition of any changes (indicated by '*' mark after filename in title bar)
  and dialog with "Save current changes?" question (when new song, load song,
  import song and/or when tracker goes to exit).
- Hotkey for "Cursor go to the track speed column" changed to Control+Z.
  (There is also new menu item "Track - Cursor go to the speed column".)
- Control+S is new hotkey for "File - Save" from now.
  (If this hotkey is used, a message box with query "Save song to file '...'?"
  appears and you have to confirm your request. Also you can disable this
  query message box in Config dialog (menu View - Configuration)).
- New hotkey Control+L for "File - Load...".
- New hotkey ScrollLock for auto-follow mode turn on/off.
- Sound click when "Undo" hotkey is pressed but undo is not possible.
- Handling of track events with zero volume during manual step replaying
  (by Shift+/Control+/Enter hotkey) corrected.
- Vertical line separator added between left and right tracks.
- View menu items settings are stored to rmt.ini configuration file.
- New functions in menu Instrument - submenu Paste special:
  * Volume envelopes and Envelope parameters only
  * Insert Volume envelopes and Envelope parameters to cursor position
    (Note: Cursor has to be in some column of volume or parameters envelope.)
  * Volume R to L envelope only
  * Volume L to R envelope only
- New function in menu "Song - Song change maximal length of tracks".
  It also compute effective maximal length value for current song.
  Warning: All tracks can be prolonged or truncated!
  (Note: Computing and setting of effective maximal length is also added
   to function "Song - All size optimizations".)
- Routine in exported XEX Atari executable msx file improved.
  Now it works well on PAL/NTSC computers and playing speed
  is adjusted automatically to 50Hz on PAL and also on NTSC systems.
  (If configuration is set to NTSC system speed, then 60Hz.
   Note: RMT and SAP files doesn't contain any NTSC type, so playing speed 60Hz
   is supported only in exported XEX Atari executable msx files.)
- NumLock handling improved (I hope ;-)).
- Other small corrections and bugfixes. 

RMT routine changes
- New variable RMTSFXVOLUME for sfx volume settings (volume * 16).
  Example "/asm_src/sfx/sfx.a65" has been modified.
  (suggested by Tebe)
  (Coders, you have to use new rmtplayr.a65)

Accessories
- Atari RMT player RMTPL107.XEX (new version 1.07) is in "player" directory.
  New features:
  * Support for manual entering Device:Filename by TAB hotkey.
    (requested by Baktra)
  * Support for up to 35 subsongs by hotkeys 1-9 and A-Z.
  * Support for PAL/NTSC computer systems. Playing speed is adjusted
    to 50Hz on PAL and also on NTSC systems.
  (There is short description in RMTPL107.TXT file.)

New songs
- Song "wyjasnijmy_to_sobie.rmt" (by LiSU)
  in "songs/lisu/" directory.
- Song "bomb_jack.rmt" (by Miker)
  in "songs/miker/" directory.
- Song "amelie.rmt" (by Nooly)
  Song "summer.rmt" (by Nooly)
  in "songs/nooly/" directory.
- Song "vietnamska_mise.rmt" (by PG)
  Song "summerdays.rmt" (by PG)
  Song "deflektor.rmt" (by PG)
  Song "xmasmix.rmt" (by PG)
  Song "gpc.rmt" (by PG)
  Song "kaviar.rmt" (by PG)
  in "songs/pg/" directory.
- Song "mab.rmt" (by Raster/c.p.u.)
  Song "indianajones4.rmt" (by Raster/c.p.u.)
  Song "3d.rmt" (by Raster/c.p.u.)
  in "songs/raster/" directory.
- Song "sunset_on_the_moon.rmt" (by XLent)
  Song "4tk35.rmt" (by Caruso)
  Song "ilusia.rmt" (by StRing)
  Song "naue.rmt" (by StRing)
  in "songs/" directory.


Changes in RMT 1.27
-------------------

- Support for another external Pokey sound emulation provided by apokeysnd.dll
  by Fox/Taquart (http://asap.sourceforge.net/apokeysnd.dll).
  Description of apokeysnd.dll functions is in the readme.txt file.
  Note: If both Pokey DLLs are placed to RMT directory, usage of apokeysnd.dll
  has higher priority than sa_pokey.dll.
- New menu "Undo" with support for Undo/Redo (up to 100 steps) and ClearHistory.
  (There is Alt+Backspace hotkey for undo operation.)
  (requested by LiSU)
- New options in Config dialog (menu View - Configuration):
  * TrackEdit cursor vertical range ... there can be selected value from 0 to 8
    for vertical cursor movement in track edit area. (default is 6)
  * Reset of Atari sound routine when ESC is pressed. (default is off)
    (suggested by Miker)
- New TrackEdit and SongEdit hotkeys:
  * Control+PageUp ... Move cursor to begin of subsong or begin of previous subsong.
  * Control+PageDown ... Move cursor to begin of next subsong.
  (suggested by Miker)
- New buttons in block effect/tool dialog (menu Block - Effect/tools or Control+F):
  * "Try" ... Perform the selected effect but no close effect dialog (Alt+T)
  * "Restore" ... Restore block to original state (Alt+R)
  * "Play/Stop" ... Play the selected block repeatedly / stop playing (Alt+P)
  (suggested by LiSU)
- Function "Change all the instrument occurrences" improved:
  * Checkbox "Only in some channels"
    with special dialog box for L1..R4 channels selection.
  * Checkbox "Only in song lines" for song lines from/to selection.
  Important note: If some tracks are used inside and outside of selected
  channels+song lines area and instrument changes should be performed,
  new tracks will be created for changes in selected area only
  and song will be adjusted automatically.
  (suggested by Sal Esquivel)
- New function "Reload" in menu Project.
  After confirmation it discards all changes since your last save.
- Prepared last used filename in "Save As" dialog.
- Support for new FEAT options.
  "File - Export As - RMT stripped song file (*.rmt)" there are two new checkboxes:
  * "GlobalVolumeFade support" (RMTGLOBALVOLUMEFADE variable)
    (requested by Dely)
  * "No starting song line" (start from song line 0 always)
- Bugfix of Path setting for loading/saving of songs/instruments/tracks.
- Bugfix of lines' coordinates in "Song columns' order change/copy/clear" dialog.
- Bugfix of default extensions addition.
- Other small corrections and bugfixes.

RMT routine changes
- New rmt_feat option FEAT_GLOBALVOLUMEFADE (+7 bytes).
  If it is activated, there is possible to control global song volume
  via RMTGLOBALVOLUMEFADE variable ($00-$f0, step $10):
  $00=normal volume (100%) ... $f0=minimal volume (0%).
  Example is in "/asm_src/volume/" directory.
- New size optimalization option supported:
  FEAT_NOSTARTINGSONGLINE (it can save 22 or 24 bytes) for song starting
  from song line 0 always (no support for song line init by A-register).
  For example "/asm_src/optim/musico.xex" is 22 bytes shorter now.
  (Coders, you have to use new rmtplayr.a65 and rmt_feat.a65)

New songs
- Song "acidjazzed_evening.rmt" (by Miker)
  Song "jetboy.rmt" (by Miker)
  Song "7_gates_of_jambala.rmt" (by Miker)
  Song "astaroth.rmt" (by Miker)
  Song "easter_chickens.rmt" (by Miker)
  Song "enchanted_lands.rmt" (by Miker)
  Song "ghosts_n_goblins.rmt" (by Miker)
  Song "logical_3.rmt" (by Miker)
  Song "menace_song.rmt" (by Miker)
  Song "torvak_level_2.rmt" (by Miker)
  Song "flimbo.rmt" (by Miker)
  in "songs/miker/" directory.
- 57 (!) songs by Kjmann (Sal Esquivel)
  in "songs/kjmann/" directory.
- Song "imsure.rmt" (by Raster/c.p.u.)
  Song "whoknows.rmt" (by Raster/c.p.u.)
  in "songs/raster/" directory.
- Song "funny.rmt" (by LiSU)
  Song "przyrada.rmt" (by LiSU)
  in "songs/lisu/" directory.
- Song "brainless.rmt" (by PG)
  Song "hammastahna.rmt" (by PG)
  Song "happy_sundays.rmt" (by PG)
  Song "jozin_z_bazin.rmt" (by PG)
  Song "radixex.rmt" (by PG)
  in "songs/pg/" directory.
- Song "aoki.rmt" (by Nooly)
  Song "hightide.rmt" (by Born/LaResistance)
  Song "astrosphere.rmt" (by Kozyca)
  Song "30minutes.rmt" (by Elan)
  Song "bazalt.rmt" (by Yerzmyey/HOOY-PROGRAM)
  Song "m4700rk4.rmt" (by Epi/Trs)
  in "songs/" directory.

New instruments
- "sharp_ch1.rti" (Sharp channel 1)
  in "instruments" directory.


Changes in RMT 1.26
-------------------

- Song path is memorized if song filename is used in command line.
- Cursor is scrolling down for preselected number of lines also after
  track editing of note volume and/or octave. (suggested by Elan)
- New instrument edit hotkeys/functions:
  Instrument envelope edit:
  * Shift+Insert ... Duplicate the current envelope column. (suggested by LiSU)
  * Space ... Clear the current envelope column values and move cursor to the right.
  Instrument table edit:
  * Shift+Insert ... Duplicate the current table item.
  * Space ... Clear the current table item and move cursor to the right.

Bugfixes
- Occasional unhandled exception error with clone track function.
- Bug in Effect/tools "Expand/shrink lines".

RMT routine changes
- Correction of crazy FEAT optimalization bug with used FEAT_COMMAND5
  and unused FEAT_PORTAMENTO. (Thanks to Wrathchild for notice.)
- Correction of crazy FEAT optimalization bug with used FEAT_COMMAND6
  and unused FEAT_FILTER.
- Some additional super-ultra-hard ;-) improvements of FEAT speed/size optimizations.
  For example "/asm_src/optim/musico.xex" is 39 bytes shorter now (and there is saved
  a lot of CPU cycles, of course).
  (Coders, you have to use new rmtplayr.a65)

New songs
- Song "shorty_noises.rmt" (by Elan /unfinished party version/ 17.3.2007)
  Song "kurczak.rmt" (by LiSU)
  in "songs/" directory.
  Song "disturbance.rmt" (by Miker)
  Song "flowers_mania.rmt" (by Miker)
  Song "my_first_one_in_rmt.rmt" (by Miker)
  Song "tempest2000_blue_level.rmt" (by Miker)
  Song "the_last_ninja_2_central_park.rmt" (by Miker)
  Song "tyrian_zanac5.rmt" (by Miker)
  Song "wings_of_death_lv2.rmt" (by Miker)
  in "songs/miker/" directory.
  Song "delight.rmt" (by Nils Feske)
  Song "nothing.rmt" (by Nils Feske)
  Song "takeoff.rmt" (by Nils Feske)
  in "songs/nilsfeske/" directory.
  Song "against_time.rmt" (by PG)
  Song "stardust_memories.rmt" (by PG)
  Song "strangled_mind.rmt" (by PG)
  in "songs/pg/" directory.
  Song "gearup.rmt" (Gear up, Forever 8 msx by raster/c.p.u. 2007)
  Song "l45tm1nut3.rmt" (L45T M1NUT3, Glucholazy 2007 msx by raster/c.p.u. 26.7.2007)
  in "songs/raster/" directory.


Changes in RMT 1.25
-------------------

- Maximal length of instrument envelope increased to 48 (ELEN=$01..$30).
  Overall size (decadic value in bytes) of Atari instrument data there is shown.
  (Note: One instrument with full envelope and note/frq table has size 188 bytes.)
  Warning: If you will load instrument files with longer envelope than 32
  (or RMT modules containing such instruments) to older RMT tracker program versions,
  it can cause totally unpredictable results (inclusive of program crash).
  Please, use the latest RMT tracker program versions always to avoid this kind of problems.
  Previous Atari RMT player programs and Atari RMT assembler player routines
  are working well all along (there isn't problem with longer instrument envelope).
- Cursor background color was improved (better visibility). (suggested by PG)
- menu Instrument - Info about using of actual instrument - Track listing
  info added and volume range detection improved (standalone volume lines are processed too).
- menu Instrument - Change all the instrument occurrences - new options:
  * "From/To instrument" ranges combo boxes.
  * "One instrument only" checkbox.
  * "Changes only in current track" checkbox. (suggested by PG)
  * "Default ranges" button.
  * "All instruments" button.
  * "The same instrument range" checkbox.
  Change process improved for standalone volume lines.
- Mouse control of turn on/off the channels in song head area (L1-R4 title)
  * LeftMouseButton ... turn on/off relevant channel.
  * RightMouseButton ... solo play of relevant channel / turn on all channels.
  (suggested by PG)
- New features in Config dialog (menu View - Configuration):
  * Remembering octaves and volumes for each instrument separately
    (default is on). (suggested by LiSU)
    Note: Instruments' octaves and volumes are also stored to RMW working file.
  * "Paths..." button.
    There is possible to define default paths for loading and saving of songs,
    instruments and tracks. Each path type is remembered separately.
    (suggested by LiSU & Miker)
- Some improvements in block effect/tool (menu Block - Effect/tools or Control+F)
  for "Echo" effect:
  Fade out level can be defined as percentage value for proportional volume degression
  or as integer value for linear volume subtraction.
  Support for echo ending on minimal volume. (suggested by LiSU)
- Some configuration parameters are stored to RMW working file.
- Suppression of Atari "Attract mode" in exported "XEX Atari executable msx".
- Other internal improvements and small bugfixes.

New songs
- Song "devils.rmt" (Atari version by PG, 2006)
  Song "krakout.rmt" (Atari version by PG, 2006)
  Song "lcd3cd3.rmt" (Atari version by PG, 2006)
  Song "somewhere.rmt" (Atari version by PG, 2006)
  in "songs/pg/" directory.
  Song "astro4road.rmt" (Astro4road game music by raster/c.p.u. 2006)
  in "songs/raster/" directory.


Changes in RMT 1.24
-------------------

- New options in Config dialog (menu View - Configuration):
  * Alternative track line numbering
    (in accordance with track line highlight step).
  * Swap "replay note / replay all notes" functions
    (Enter / Control+Enter hotkeys).
  (both options requested by LiSU)
- Support for mouse-wheel control (according to the current
  mouse cursor position):
  * Song scroll up/down
  * Tracks scroll up/down
  * Instrument previous/next
  * Volume up/down
  * Octave up/down
- Bookmark support - toolbar button "Play from bookmark".
  (Button is disabled if bookmark is not set up).
  New hotkeys:
  * Control+F1 ... Set bookmark to the current position
    (also current beat speed is stored).
  * F1 ... Play song from the bookmark position (only if bookmark is set up)
    include beat speed initialization by stored bookmark beat speed.
  * Shift+F1 ... Similar to F1, but with auto-follow of the currently played position.
  Note: If insert/delete song line operations are performed,
  bookmark is moved down/up automatically.
- Other small corrections and bugfixes.

RMT routine changes
- New speed/size optimalization options supported:
  FEAT_EFFECTVIBRATO (it can save 65 bytes and quite a lot of CPU cycles).
  FEAT_EFFECTFSHIFT (it can save 11 bytes and quite a lot of CPU cycles).
  btw - if no one from this two effects is used, it can save 102 bytes
  and a lot of CPU cycles. If FEAT_VOLUMEMIN isn't used too, another 2 bytes
  will be saved.
- Other speed optimizations with vibrato effect and with RMT working registers
  (19 bytes shorter code and a lot of CPU cycles saved).
  (Coders, you have to use new rmtplayr.a65 and rmt_feat.a65)

Accessories
- Atari RMT player RMTPL106.XEX (new version 1.06) with playing of subsongs
  feature is in "player" directory.
  (There is also short description in RMTPL106.TXT file.)

New songs
- Song "satellit.rmt" (Satellite One by Purple Motion, Atari version by raster/c.p.u. 2006)
  Song "threeht.rmt" (Three Hundred Thirteen - raster/c.p.u., 2006)
  Song "glu.rmt" (GLU - raster/c.p.u., 2006)
  Song "gerappaa.rmt" (Gerappppaa - LiSU, 2006)
  Song "thrust.rmt" (T'H'R'U'S'T - Centy Brown, 2006)
  in "songs" directory.


Changes in RMT 1.23
-------------------

- Improvements in MODule import function (with tone-portamento effect).
- Support for SFX effects.
  "File - Export As - RMT stripped song file (*.rmt)" there is improved
  export dialog with "SFX support" checkbox.
  You can use instruments for sound effects in your programs -
  - example is in "/asm_src/sfx/" directory.
- Better play mode switching to "Play from currently edited position"
  if song is playing with "follow song" already. (Sounds aren't interrupted.)
- Some internal improvements.
  (rmt_ata.sys,rmt_msx.sys,rmt_sap4.sys and rmt_sap8.sys system data files
  are included in RMT tracker executable.)

RMT routine changes
- New option FEAT_SFX for SFX support feature. RMT routine is a bit longer
  and slower with SFX support enabled, so, enable it only if you will
  use RMT routine also for SFX effects.
  (Coders, you have to use new rmtplayr.a65 and rmt_feat.a65)

New song
- Song "commando.rmt" (Commando - sack/cosine, 2003. original by Rob Hubbard)
  in "songs/sack_cosine" directory.


Changes in RMT 1.22
-------------------

- Block effect/tool "Volume humanize" improved - better random values. ;-)
- New block effect/tool (menu Block - Effect/tools or Control+F
  or toolbar button "FX") function: "Volume set/remove".
  It allows to set volumes or remove whole note events
  according to the current volume values.
- New function in Song menu:
  * Make track's duplicate and put it to actual song pos. (Control+M)
    (It also check if track is used more times in song. If doesn't, message box
    with question appears.)

RMT routine changes
- Several improvements of RMT Atari assembler routine - it is 5 bytes shorter now
  and save some a few CPU cycles.
- New speed/size optimalization options FEAT_INSTRSPEED (it can save up to 21 bytes
  and some CPU cycles) and FEAT_CONSTANTSPEED (it can save 28 bytes and some CPU cycles).
  (Coders, you have to use new rmtplayr.a65 and rmt_feat.a65)


Changes in RMT 1.21
-------------------

- Enhanced mouse control:
  * Set cursor position in track edit area, song edit area,
    instrument parameters / envelope / table area, info / speed area.
  * Octave selection by mouse (click to "OCTAVE x-y" text).
  * Volume selection by mouse (click to "VOLUME x" text).
    (Also you can turn on/off "respect volume" option there.)
  * Instrument selection by mouse (click to "XX: instrument name" text).
  * Envelope GO/LEN parameter setting area (click to mouse L/R button).
  * Table GO/LEN parameter setting area (click to mouse L/R button).
    (Areas' locations - see the screenshots.)
- New hotkeys:
  SongEdit:
  * Enter ... Exit from SongEdit section.
  * Home ... Move cursor to first song line.
  * End ... Move cursor to last song line.
  * PageUp ... Move cursor 4 song lines up.
  * PageDown ... Move cursor 4 song lines down.
- New option in Config dialog (menu View - Configuration):
  * Don't use hardware sound buffer
    (default is off).
    (Maybe it could help if you have some system sound related problems.)


Changes in RMT 1.20
-------------------

- Automatic notes' start-time quantization to the nearest beat
  during the real-time recording. (Note events incoming after half of beat
  are delayed and stored to the next track line.)
- "File - Export As" contains new type "ASM simple notation source (*.asm)".
  You can adjust a lot of options in dialog box will be showed there.
  It's useful for some easy music and/or sounds effects in your tiny programs -
  - example is in "/asm_src/simple/" directory.
- Some other small internal corrections.

Accessories
- Atari RMT player RMTPL105.XEX (new version 1.05) is in "player" directory.
  (There is also short description in RMTPL105.TXT file.)

RMT routine changes
- Some corrections and speed optimizations in RMT assembler routine.
  It is 6 bytes longer now, but quicker about 20 CPU cycles..

New songs
- Song "ramaja.rmt" from Tatqoo/Taquart in "songs/tatqoo" directory.
- Song "itdoesnt.rmt" (It Doesn't Matter, raster/c.p.u., 2005)
  Song "paulthep.rmt" (Paul the penguin by radix, Atari version by raster/c.p.u., 2005)
  Song "cervi2.rmt" (Cervi2, by raster/c.p.u., 2005)
  Song "cubico.rmt" (Cubico music & sfx, Atari version by raster/c.p.u., 2004)
  in "songs" directory.


Changes in RMT 1.19
-------------------

- New combo box in main toolbar for setting number of lines (from 0 to 8)
  cursor will scroll down after entering notes or pressing space key.
  Also new hotkeys for control of this parametr:
  NumLock ... +1, Shift+NumLock ... -1
- Function "Song - Song columns' order change"
  was changed to "Song - Song columns' order change/copy/clear"
  and now there is possible to use it also for copying or clearing
  of song columns. There are also some new options:
  * button "Copy left-->right"
  * button "Copy left<--right"
  * button "Clear all"
  * range parameters "From song line $:__" and "To song line $:__"

New songs
- 11 songs from Tatqoo/Taquart in "songs/tatqoo" directory.
- Song "smells.rmt" (Smells like teen spirit, Atari version by sack/c0s, 2004)
  in "songs/sack_cosine" directory.


Changes in RMT 1.18
-------------------

Important bugfix
- Bad keys recognition when Numblock keys autorepeat function was active.

New songs
- Song "timett.rmt" (Time to turn, raster/c.p.u., 2004)
  in "songs" directory.
  (SAP file "timett.sap" is in "exports/sap" directory.)


Changes in RMT 1.17
-------------------

- Menu Track:
  * Load and paste to actual track.
  * Save actual track as...
  * Clear all duplicated tracks, adjust song.
    (Note: This function is also included in Song - All size optimizations.)
- New block effect/tool (menu Block - Effect/tools or Control+F
  or toolbar button "FX") function: "Modify notes, instruments and volumes".
- Function "Song - Prepare song line with unused empty tracks (Control+P)"
  was changed to "Song - Insert new line with unused empty tracks (Control+P)"
  and now it insert this new song line into actual song line positions,
  not to line below.
- Function "Song - Prepare duplicated song line (Control+O)"
  was changed to "Song - Insert copy or clone of song line(s) (Control+O)".
  and now it shows new copy/clone dialog for setting of parameters
  for this very helpful function.
- Menu Song:
  * Put new empty unused track to actual song position (Control+N).
- "File - Export as XEX Atari executable msx" there is possible to:
  * define 4+1 lines now. 5th line is showed instead of 4th line 
    if Atari SHIFT key is pressed.
  * define color of rasterline meter.
- "File - Export as SAP file" there is possible to define hexadecimal song line
  numbers for more SAP module subsongs. Initial values are prepared by automatic
  subsong-detection algorithm, but you can change it at pleasure.
- Added "*.tm8" extension in Import TMC open file dialog.
- New option in Config dialog (menu View - Configuration):
  * F1-F8 keyboard layout:
    - RMT original keyboard layout.
    - Layout 2:
      F1=track edit,F2=instrument edit,F3=info edit,F4=song edit
      F5=Play,F6=Play from start,Control+F7=Play tracks,F7=Replay tracks,F8=Stop
      (This layout is for F5-F8 keys identical with some other renowned
       module tracker programs.)

Accessories
- Atari RMT player RMTPL104.XEX (new version 1.04) is in "player" directory.
  (There is short description in RMTPL104.TXT file.)

New songs
- Song "hexxagon.rmt" (Hexxagon music & sfx, raster/c.p.u., 2003)
  Song "basix.rmt" (Basix, by raster/c.p.u. 2004)
  Song "gemx.rmt" (Gem'x song, Atari version by raster/c.p.u. 2004)
  in "songs" directory.
  (SAP file "hexxagon.sap" with 6 subsongs is in "exports/sap" directory.)
  (SAP file "basix.sap" with 9 subsongs is in "exports/sap" directory.)

Bugfixes
- Revision of keyboard control. 
  NumLock mode is turned off all the time when tracker main window is active,
  so that key combinations Shift+NumBlockKeys are working as well as state
  of right Shift key can be correctly detected now.
- Internal improvement of TMC import algorithm (better recognition of identical
  tracks arised from other tracks by song shift parameter).
- If tracker window isn't active, MIDI events on channel 0 are ignored.
- Other small corrections and bugfixes.


Changes in RMT 1.16
-------------------

- Instrument edit
  * Shift+Control+Num+,Num- ...change the R+L volume envelopes up/down.
    If cursor is at line "VOLUME L" or "VOLUME R", then volume envelope
    for left or right channel only is changed.
- Menu Block - submenu Paste special:
  * Merge with current content
  * Volume values only
  * Speed values only
- Menu Instrument - submenu Paste special:
  * Volume R+L envelopes only
  * Volume R envelope only
  * Volume L envelope only
  * Envelope parameters only
  * Table only
- Preserve last "export type" in "Export As" dialog.

RMT routine changes
- A lot of great speed/size optimizations made by Jaskier/Taquart.
  Thank you very much, Jaskier!!!
- A few another speed/size optimizations.
- RMT routine is 40 bytes shorter now and quicker about 100-200 CPU cycles.


Changes in RMT 1.15
-------------------

- Quadruple instrument speed allowed also for 8 tracks STEREO modules.
- .rmt (.txt,.rmw) filename can be used as the command line parameter
  for the automatic loading of this file after start of the RMT tracker.
  (It's recommended to make association for "rmt" extension to run RMT tracker.)
- Show Play time counter (from 00.0 sec to 9:59 minutes).
  (default is on, turn off by menu View - Play time counter)
- Menu Track:
  * Info about using of actual track.
  * Renumber all tracks (2 types of ordering).
  * Clear all tracks unused in song.
- Menu Instrument:
  * Renumber all instruments (3 types of ordering).
  * Clear all unused instruments.
- Two tracks functions moved from Song menu to Track menu (Search and rebuild wise 
  loops in all tracks, Expand loops in all tracks).
- Size optimization function (menu Song - All size optimizations) perform also
  clearing of all unused instruments and renumbering of all tracks and instruments now.
  (So there can be a bit better result of size optimization thanks to removing unused
  instruments and removing empty gaps after deleting of unused tracks and instruments.)
- Preserve last "From address" value in export of stripped RMT file dialog.

RMT routine changes
- Several improvements of RMT Atari assembler routine - it is 22 bytes shorter now
  and quicker about 50-150 CPU cycles.
- New speed/size optimalization options FEAT_VOLUMEMIN and FEAT_TABLEGO supported 
  and optimalization for FEAT_AUDCTLMANUALSET improved.
  (Coders, you have to use new rmtplayr.a65 and rmt_feat.a65)
- New mono/stereo compile modes. There is variable STEREOMODE (instead of previous
  variable STEREO8T) with the following values:
  STEREOMODE equ M  ;M=0 => compile RMTplayer for 4 tracks mono
                    ;M=1 => compile RMTplayer for 8 tracks stereo
                    ;M=2 => compile RMTplayer for 4 tracks stereo L1 R2 R3 L4
                    ;M=3 => compile RMTplayer for 4 tracks stereo L1 L2 R3 R4
  * Note: If RMTplayer routine compiled with STEREOMODE 2 or 3 is used on Atari computer 
    without STEREO upgrade, then standard 1 POKEY mono music will be played.
    It exploit POKEY memory area mirroring - there isn't used any "stereo detection" method!

Bugfixes
- Bug in rmtplayr.a65 initialization part if nonzero "starting song line position"
  was used. (This problem occurred in some cases only.)
- Buffer overflow error with export of stripped RMT file for too high address.
- Other small corrections and bugfixes.


Changes in RMT 1.14
-------------------

- Menu Project: Import...
  * Support for importing of classic ProTracker MOD format with 31 samples, 
    4 or 8 channels (also modules with 5, 6, 7 channels), as well as old 
    ProTracker 15 samples 4 channels MOD modules.
    After import there is need of manual adjustment (tones tuning and distortions) 
    of all instruments at first!!! Volume envelopes and loops are prepared 
    in accordance with real samples in MOD module.
  * Support for importing of Atari XE/XL TMC Theta Music Composer modules.
    TMC instruments are imitated automatically by RMT instruments if it is possible.
    There can be need of manually corrections!!! 
    (Disclaimer: Some TMC effects may be totally wrong, sorry.)
- Menu Track:
  * Search and build wise loop.
  * Expand loop.
- Menu Instrument:
  * Info about using of actual instrument.
  * Change all the instrument occurrences.
- Menu Song:
  * Song switch to 8 tracks / Song switch to mono 4 tracks.
  * Tracks' order change in whole song.
  * Search and rebuild wise loops in all tracks.
  * Expand loops in all tracks.
  * Size optimization.
- Menu View - Configuration:
  * Continue on previous/next song track upon the first/last track line
    (default is on).
- Insert/delete envelope columns in instrument edit mode
  by Insert/Delete key (at the current cursor position).
- Insert/delete table items in instrument edit mode
  by Insert/Delete key (at the current cursor position).
- Increase/decrease the whole envelope row of parameters in instrument edit mode
  by Shift+Control+Up/Down key (while cursor is in envelope data area).
- Increase/decrease the whole instrument table in instrument edit mode
  by Shift+Control+Up/Down key (while cursor is in table data area).

Accessories
- Atari RMT player RMT1PLAY.XEX (new version 1.01) is in "player" directory.
  (There is possible to show/hide song info text by spacebar key.)

New songs
- Song "nopromis.rmt" (No promises, raster/c.p.u. 2003)
  Song "aurora_s.rmt" (Hymn to aurora, Atari version by raster/c.p.u. 2003)
  Song "turrican2_rev2s.rmt" (Turrican 2 noise 3, Atari version by raster/c.p.u. 2003)
  in "songs" directory.
  (SAP file "aurora_s.sap" and "turrican2_rev2s.sap" in "exports/sap" directory.)
- 5 songs from sack/cosine, 2003, www.cosine.org.uk
  in "songs/sack_cosine" directory.

New instruments
- "drums/bassdrum.rti"
  "drums/snaredrum.rti"
  "drums/hithatclose.rti"
  "drums/hithatopen.rti"
  in "instruments" directory.

Bugfixes
- Main volume changes in instrument edit mode.
- Other small corrections and bugfixes.


Changes in RMT 1.13
-------------------

- MIDI multitimbral playing possibilities.
  Now you can use the RMT like a Atari multitimbral MIDI instrument.
  You have to send MIDI output from your MIDI sequencer or player 
  to RMT MIDI input by means of some virtual MIDI cable (for example 
  "MIDI Yoke" etc.). MIDI implementation chart is in midi.txt file.
- All tracks cleanup (menu Tracks - All tracks cleanup).
- All instruments cleanup (menu Instrument - All instruments cleanup).
- Block effect/tool "Expand/shrink lines" improved - negative values allowed
  for bottom-up way alterations.
- Prepare duplicated song line (menu Song or Control+O).
- Prepare song line with unused empty tracks (menu Song or Control+P).
- Other small corrections.


Changes in RMT 1.12
-------------------

- Support for higher instrument speeds: triple speed for STEREO modules,
  triple and quadruple speed for MONO modules.
- Support for NTSC 60Hz system speed (menu View - Configuration).
- Simpler and shorter XEX Atari executable msx export file (*.xex).
- New menu "Block" with all the block functions.
- New feature - block effects/tools (menu Block - Effect/tools or Control+F
  or toolbar button "FX").
  Effects/tools:
  * Fade in/out
  * Echo
  * Expand/shrink lines
  * Volume humanize
- Midi event "note off" recording possibility (menu View - Configuration).

Bugfixes
- Correction of instrument tempo-related bug (in RMT assembler routine
  and also in tracker). (Thanks to Memblers from Indiana for notice.)
- Higher instrument speeds timing in Atari msx corrected.
- Numeric block keys plus/minus locked away the shift key.


Changes in RMT 1.11
-------------------

- Maximal length of track increased to 256 beats.
- New Config dialog (menu View - Configuration):
  * Track line highlight step.
  * Some keyboard hotkeys options.
  * MIDI IN support! (events: key on, program change)
  * OK button save the configuration to rmt.ini configuration file.
- New input/output file format: TXT song files (*.txt).
  (It's simple text format for easy making any convert tools.)
- Change of toolbar icon for PROVE MODE and new icon for MIDI on/off
- New track edit block function:
  Control+E ..exchange of block select data and the clipboard data.
- Show the AUDF, AUDC and AUDCTL Pokey registers
  (default is off, turn on by menu View - Pokey chip registers)
- Light blue color of volume only values in the instrument envelope.
- Checking for right version of RMW working files (RMW work files haven't
  portability through the different RMT tracker versions).

New songs
- Song "aspir332.rmt" (Aspiration 332, by Raster/C.P.U. 2003) 
  in "songs" directory. 

Bugfixes
- Bug with bad beat count if standalone speed command has been in track.
- Other small corrections and bugfixes.


Changes in RMT 1.1
------------------

- Main RMT assembler routine changed to version 01:
- Backward compatibility:
  * Support for loading of previous (version 00) RTI instruments.
    (saving RTI instruments version 01 only)
  * Support for loading of previous (version 00) RMT modules.
    (saving RMT modules version 01 only)
- New RMT routine features:
  * Support for manual AUDCTL settings (but also automatic management is preserved,
    if you will use command 6 or "filter" switch).
  * Support for VOLUME ONLY settings by command 7 with parameter value $80.
    (Volume only forced outputs are indicated by light blue color of volume analyzer box,
    see the easy example song file "examples/volumeonly.rmt".)
  * Support for NOTES/FRQS TABLE length from 1 to 32.
  * A lot of speed/size optimizations (RMT modules with easy instruments are smaller than before).
- Support for speed/size optimizations of RMT assembler routine for concrete RMT module.
  (See the RMT player routine assembler source code and "Export song as.. RMT stripped
   song file (*.rmt)" dialog and optimized routine example in "asm_src/optim" directory.)
- Hotkeys' changes:
  Pause ... mute all sounds and reinit RMT routine only.
  Shift+Pause ... mute all sounds, reinit RMT routine and sound output.
- If any instrument parameter is changed, then turn off this instrument on all
  channels where it sounds.
- If save or export process is aborted or failed, destination file will be deleted.

Bugfixes
- RMT routine - instrument table speed bug (each first note/frq table item was shorter by 1/50 sec).
- Tab key while edit the instrument name (no CapsLock off).
- PageUp/Down for octave up/down in Prove mode.
- Instrument volume curve fault by mouse position after the instrument load.
- Mouse double-click to track/channels turn on/off.
- All the showed messages (message boxes) are owned by RMT application now.


Changes in RMT beta 1.02
------------------------

Since the RMT beta 1.02 version all the parts based on GPL sources are totally removed
from the RMT project (i.e. Pokey sound emulation and Atari 6502 processor emulation)
and these aren't a part of RMT henceforth. 

Description of dynamic DLL functions for standalone Pokey and 6502 emulation 
is specified in the readme.txt file. If you run RMT without this way described DLLs
(sa_pokey.dll, sa_c6502.dll), RMT will work, but there won't be any Pokey sound output
and Atari sound routines won't be executed.

Improvements
- Use the hardware sound buffer if possible (instead of software sound buffer).
- Initialize the file load directory to program location.
- Block toolbar is showed by default.
- Much more summary information in the About box.
- Text parameters editing was changed a bit. Because of ShiftKey is used for playing
  the notes, You have to press the CapsLock key for switch to "normal" ShiftKey behaviour
  (i.e. ShiftKey+Key for entering uppercase and other (!@#$%^&*()_+:"<>?|) characters).
  CapsLock mode is turned off automatically when text editing is over by pressing the Tab key
  or Enter key.
- CapsLock state indication in the status bar ("CAP").
- Reinit also RMT Atari sound routines when the Pause key is pressed.
- Functions for turn on/off the sound channels (tracks):
  * Control+ 1-8 ...turn on/off the sound channel (track) 1-8.
  * Control+ 9 ...turn on all the channels (tracks).
  * Control+ 0 ...turn off all the channels (tracks).
  * Control+ BackQuote ...turn on/off the active sound channel (track).
  * Shift+ BackQuote ...negation of turned on/off sound channels (tracks).
  * BackQuote ...solo play of active sound channel (track) / turn on all channels (tracks).
  Note: Muted sound channels are indicated by gray color of track title and volume
  analyzer scope.
- Status bar tool tips and short flying help messages completed.
- Other internal improvements.

TrackEdit
- Mouse control of turn on/off the channels (tracks) - it's indicated by mouse cursor design
  change to "hand with title 'on/off'":
  * LeftMouseButton ... turn on/off relevant channel (track).
  * RightMouseButton ... solo play of relevant channel (track) / turn on all channels (tracks).

InstrumentEdit
- Drawing of envelope volume curve by mouse - it's indicated by mouse cursor design 
  change to "pen with title 'volume'":
  * LeftMouseButton ... draw volume curve.
  * RightMouseButton ... set volume to zero value.

Bugfixes
- Playing the block area.
- Inserting and deleting song lines from track edit when "go to line" is active.
- Other internal corrections.


Changes in RMT beta 1.01
------------------------

All modes
- Set the octave up-down hotkeys changed from Shift+up,down to Shift+page_up,page_down
  (it works now in TrackEdit and InstrumentEdit only).
- Correction of bug with instrument copy/paste.

TrackEdit
- No more set number of the track by Control+0-9A-F keys.
- Hotkey changed: Control+I - insert new line into the song.
- Hotkey changed: Control+U - delete the current line from the song.
- Show title "EMPTY" for empty tracks.
- Shift+up,down,home,end - the block select functions.
- Control+A - select all data in the track (from the begin to the end of the track).
- Control+Insert,Control+C - copy block to clipboard.
- Shift+Insert,Control+V - paste data from clipboard.
- If the block is selected and paste function is used, then data from clipboard is placed into the block select area only.
- If the block is selected, then Delete key will delete data inside the block area.
- If the block is selected, then Control+X key will cut data inside the block area.
- New toolbar for block data modifications: menu View - Block toolbar. All this modify functions are available also by hotkeys.
- Block data modification hotkeys (the block must be selected at first):
  * Shift+Control+A switch between block modify mode all / current instrument only.
  * Shift+Control+page_up,page_down do transpose up, down notes in the block (by semitones).
  * Shift+Control+left,right do change of instrument numbers in the block.
  * Shift+Control+Num-,Num+ do change of volumes in the block.
  Note transpositions, instrument changes and volume changes are indicated in window statusbar at the bottom.
- If the block is selected:
  * Enter key will play the lines inside the block around.
  * Home or End key set the cursor to the first or the last line in the block.
  * Control+B will restore the block data changed by modification functions from the backup (backup is created when the block is getting start to select and destroyed when the block is deselected).
  * Control+F4 will start cyclic playing of the part from the top to the bottom of selected block area.

Operation manual
- All this changes included to operation manual (see the rmt_en.htm, rmt_cz.htm).


RMT beta 1.00
-------------

- The first published version.
