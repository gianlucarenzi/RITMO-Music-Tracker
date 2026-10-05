<p align="center">
  <img src="doc/logo/ritmo-logo.png" alt="RITMO - Atari POKEY music tracker" width="720">
</p>

# RITMO - Atari POKEY music tracker

### About

RITMO (*RITMO Is a Tracker for Music On Atari*) is a tool for making Atari
XL/XE music for the POKEY sound chip. It is a fork of RASTER Music
Tracker (RMT), which uses the Atari XL/XE music routines created by
Radek Štěrba from 2002 to 2009. It was a small revolution for all Atari
musicians and fans. *Ritmo* means "rhythm" in Italian, Spanish and Portuguese.

**RITMO 2.x is the port of RMT to Qt6, so that it runs natively on Linux,
Windows and macOS** (Intel and Apple Silicon), and not only on Windows.
(2.0 was built with Qt5, 2.1 moved to Qt6.) The program was called RASTER
Music Tracker / RMT up to 2.2.1; the file formats (`.rmt`, `.rmw`) and the
RMT drivers keep their names.

**Why a new name.** RITMO is a fork of the RMT project by Peter Dell (JAC!),
to whom I am infinitely grateful: everything here stands on his work and on
that of Radek Štěrba and VinsCool. The name was changed to avoid any possible
confusion or problem with the original RMT, and to be free to remove the MFC
(Windows-only) parts of the code over time, which the upstream project keeps.

The heart of the program is **the original RMT code**: the tracker, the
editor, the Atari 6502 music routines, the POKEY emulation, the file formats,
the import and export are those of **RMT 1.35**, the development version by
Peter Dell (JAC!), itself the continuation of the original RMT 1.28 by Radek
Štěrba (Raster/C.P.U.) and of RMT 1.34 by Vin Samuel (VinsCool). The port
replaces only what tied RMT to Windows:

- the windows, menus, toolbars and dialogs (MFC in the Windows version) are now Qt6 (the tracker
  screen itself is drawn by the original code, pixel for pixel)
- the sound goes out through PortAudio, MIDI IN comes in through RtMidi
  (ALSA on Linux, WinMM on Windows, CoreMIDI on macOS)
- the 6502 and POKEY emulation is built in, no `sa_c6502.dll` /
  `apokeysnd.dll`
- the window opens at 1366x768 (a small laptop screen, where a mono song is shown whole together with the POKEY registers) or the largest
  size the screen has, remembers its size and position, and the first start chooses the interface size (100, 200 or 300 %) from the size of
  the screen; `--scale=N` sets it for one session
- the configuration is kept per user (QSettings), so it survives updates;
  the settings of RMT (`rmt.conf`, `rmt.ini`) are taken over at the first start

Songs, instruments and tracks are the same files as in RMT 1.3x: you can
move your work between the Windows version and this one.


### Documentation

The [**RITMO User Manual (PDF)**](doc/manual/ritmo-manual.pdf), with screenshots of the windows, the menus and the
dialogs, explains the program from the first song to the scripting and the export formats. It is written in
LaTeX and built by `doc/manual/build.sh` (XeLaTeX and pandoc; `--screenshots` takes the screenshots from the
program itself). The reference documents are also in `doc/` as Markdown.


### Screenshots

*High Tide* by Born/LaResistance, a stereo song (8 tracks), playing on Linux:

![RITMO playing a stereo song on Linux](doc/screenshots/ritmo-playing-linux.png)

The instrument editor:

![The instrument editor](doc/screenshots/ritmo-instrument-editor.png)

The Windows version, built by GitHub Actions (the picture is taken by the start-up test of the
package, which runs without a display):

![RITMO on Windows](doc/screenshots/ritmo-windows.png)

A dialog of RITMO (song columns' order) and the Apple Silicon version on macOS, also built
by GitHub Actions:

<p>
  <img src="doc/screenshots/ritmo-qt-dialog.png" alt="A dialog: song columns' order" width="45%">
  <img src="doc/screenshots/ritmo-macos-arm64.png" alt="RITMO on macOS (Apple Silicon)" width="52%">
</p>

More pictures of the windows, the menus and the dialogs are in the [manual](doc/manual/ritmo-manual.pdf).


### Please try it and tell me what you think!

This is the first release of the port. It has been used and tested mostly on
Linux; the Windows and macOS packages are built automatically and pass a
start-up test, but they have seen very little real use yet. **Any feedback is
very welcome**: does it start, does it sound right, do your songs load and
play as in RMT 1.3x, does MIDI work with your keyboard, is anything missing or
different from the Windows version you know?

- Open an [issue on GitHub](https://github.com/gianlucarenzi/RITMO-Music-Tracker/issues)
  (bugs, differences from RMT 1.3x, ideas)
- Please tell which package and system you used (e.g. "AppImage on Debian
  12", "DMG x86_64 on macOS 13"), and attach the song if it is about a song


### Download

[**RITMO 2.4**](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases/tag/v2.4)
([all releases](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases)):

| System | Package | How to start it |
|--------|---------|-----------------|
| Linux x86_64 (Debian 11, Ubuntu 20.04 and newer) | [`Ritmo-Linux-x86_64.AppImage`](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases/download/v2.4/Ritmo-Linux-x86_64.AppImage) | `chmod +x Ritmo-Linux-x86_64.AppImage` and run it (needs `libfuse2`; without it: `--appimage-extract-and-run`) |
| Windows 64 bit, installer | [`Ritmo-Windows-x64-Setup.exe`](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases/download/v2.4/Ritmo-Windows-x64-Setup.exe) | run it (Start menu entry, optional desktop icon and `.rmt` file type; not signed: Windows may ask to confirm) |
| Windows 64 bit, ZIP | [`Ritmo-Windows-x64.zip`](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases/download/v2.4/Ritmo-Windows-x64.zip) | unzip it anywhere and run `Ritmo.exe` |
| macOS 12 or later, Apple Silicon | [`Ritmo-macOS-arm64.dmg`](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases/download/v2.4/Ritmo-macOS-arm64.dmg) | drag `Ritmo.app` to Applications; the first time open it with right click, then Open (not notarized) |
| macOS 12 or later, Intel | [`Ritmo-macOS-x86_64.dmg`](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases/download/v2.4/Ritmo-macOS-x86_64.dmg) | as above (also runs on Apple Silicon through Rosetta 2) |
| Manual | [`Ritmo-User-Manual.pdf`](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases/download/v2.4/Ritmo-User-Manual.pdf) | the user manual (it is also [in the repository](doc/manual/ritmo-manual.pdf)) |

From 2.5 the [releases](https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases) also have two more Linux packages:

| System | Package | How to start it |
|--------|---------|-----------------|
| Linux aarch64: Raspberry Pi 3/4/5 with a 64 bit system (Raspberry Pi OS 13 trixie, Debian 13, Ubuntu 24.04 and newer) | `Ritmo-Linux-aarch64.AppImage` | as the x86_64 AppImage |
| Linux riscv64 (Debian 13 and newer) | `Ritmo-Linux-riscv64.tar.gz` | unpack it, install the packages its `README.txt` lists (`sudo apt install ...`: only the graphics and sound libraries), run `./ritmo` |
| Linux ppc64, PowerPC 64 bit big endian (Debian ports, sid) | `Ritmo-Linux-ppc64.tar.gz` | as the riscv64 one |
| Linux riscv64 and ppc64, Debian | `ritmo_<version>_riscv64.deb`, `ritmo_<version>_ppc64.deb` | `sudo apt install ./ritmo_<version>_<arch>.deb` (apt installs Qt 6, PortAudio and RtMidi too), then `ritmo` |

The packages up to 2.2.1 were called `RMT-*` (the program `Rmt.exe`, `rmt`); from 2.3 they are `Ritmo-*` with the program `ritmo` / `Ritmo.exe`.

Everything the program needs is inside each package (the riscv64 and ppc64
archives carry Qt, PortAudio and RtMidi, and use of the system only glibc and
the graphics and sound libraries of the machine). The configuration is
kept in `~/.config/ritmo-atari.org/ritmo.conf` on Linux, in the registry
(`HKEY_CURRENT_USER\Software\ritmo-atari.org\ritmo`) on Windows and in
`~/Library/Preferences/org.ritmo-atari.ritmo.plist` on macOS.

The original Windows (MFC) versions of RMT are still available from the
upstream project:
- [Latest daily build of 1.35](https://www.wudsn.com/productions/windows/rastermusictracker/rmt135-daily.zip)
- [Stable version 1.34 (2023-03-10)](https://www.wudsn.com/productions/windows/rastermusictracker/rmt134.00-stable.zip)
- [Stable version 1.28 (2009-05-19)](https://www.wudsn.com/productions/windows/rastermusictracker/rmt128.zip)


### What is different in the port

- Everything of the Windows version is there: all the menus, the dialogs
  (configuration, tuning, export and import options, block effects, song and
  instrument tools...), the two toolbars, MIDI IN with MIDI on/off, the
  play modes and the keyboard shortcuts.
- MIDI IN devices are the ones of the system (on Linux the ALSA sequencer
  ports, e.g. a USB keyboard or a virtual port).
- Settings of an earlier installation of the port (`rmt.ini` / `tuning.ini`
  next to the program) are taken over at the first start.

Known limits: the packages are not signed; the macOS and Windows versions
need testing on real machines, sound and MIDI in particular.


### Building from source

On Linux:

```bash
sudo apt install cmake qt6-base-dev portaudio19-dev librtmidi-dev
cmake -B build-qt -DCMAKE_BUILD_TYPE=Release
cmake --build build-qt -j
./build-qt/out/ritmo song.rmt
```

[BUILD.md](BUILD.md) has all the details: the other platforms, the
AppImage (`scripts/build-appimage.sh`, x86_64 and aarch64), the archive of the
other Linux architectures (`scripts/build-tarball.sh`), the GitHub workflows that build the
packages, the test hooks, and the state of every part of the Qt frontend.
RITMO builds with Qt only: the MFC (Windows-only) code of RMT was removed.


### Documentation

- Current [RMT 1.35 Documentation](https://html-preview.github.io/?url=https://github.com/peterdell/RASTER-Music-Tracker/blob/dev/doc/rmt_en.html) (it applies to the port too)
- Original [RMT 1.28 documentation](https://html-preview.github.io/?url=https://github.com/peterdell/RASTER-Music-Tracker/blob/dev/doc/rmt_en_128.html)
- The [change history](https://github.com/peterdell/RASTER-Music-Tracker/blob/dev/doc/rmt_changes.md) and the [versions](https://github.com/peterdell/RASTER-Music-Tracker/blob/dev/doc/rmt_versions.md) of RMT

Technical Documentation
- Current [RMT Tracker documentation](https://github.com/peterdell/RASTER-Music-Tracker/blob/dev/doc/rmt_tracker.md) and discussion
- Current [RMT Module File Format documentation](https://github.com/peterdell/RASTER-Music-Tracker/blob/dev/doc/rmt_format.md) and discussion


### Main features

Note that this is as of RMT 1.28 and not accurate for 1.34 and later!

* Mono 4 tracks / stereo 8 tracks.
* 254 tracks, each with its own length (256 beats max.) and with support for track loop.
* 64 instruments (stereo, instrument table up to 32 steps - 2 types and 2 modes with loop,
  instrument envelope up to 32 steps with loop, portamento, filter, 16bit bass, volume slide,
  volume minimum, vibrato, frequency shifting, etc.).Fully automatic management of AUDCTL
  register (filters, 16bit basses) and/or manual AUDCTL settings.
* Support for "volume only" forced output.
* Note portamento up/down effect.
* Instrument envelope commands for note/frequency shifting and support for special 
  "like a C64 SID chip" filtering.
* Up to 256 lines for song (with "goto line" support).
* Beat speed 1 to 255 (1/50 to 255/50 sec).
* Instrument speed from 1 to 4 per screen (up to 1/200 sec).
* Main input/output song file format: RMT song files (*.rmt).
* Input/output instrument file format: RMT instrument files (*.rti).
* Export formats: RMT stripped song file (*.rmt), SAP file (*.sap),
  XEX Atari executable MSX file (*.xex), ASM simple notation source (*.asm).
* Import formats: ProTracker modules (*.mod), Atari XE/XL Theta Music Composer songs (*.tmc)
* Support for speed/size optimizations of RMT assembler player routine 
  for a concrete RMT module (very useful for background music in demos, games, etc.).
* MIDI IN support!
* MIDI multitimbral playing possibilities.
  You can use the RMT like an Atari multitimbral MIDI instrument. 
  You have to send MIDI output from your MIDI sequencer or player 
  to RMT MIDI input by means of some virtual MIDI cable (on Linux an ALSA
  sequencer port, on Windows for example "loopMIDI", on macOS the IAC
  driver). The MIDI implementation chart is in the [midi.txt](doc/midi.txt) file.


### Known Issues

On Linux, programs running in the background that hold the audio output take
processor time from the sound of RITMO: on a slow computer this can be heard
as glitches. One is often there without being asked for: **FluidSynth**, a MIDI
synthesizer that some distributions start at every login. RITMO does not need
it (it only receives MIDI, and a MIDI keyboard works without it); stop it while
you make music with `systemctl --user stop fluidsynth`, or for good with
`systemctl --user mask fluidsynth` (undo: `systemctl --user unmask fluidsynth`).
If the sound still crackles or is distorted, choose a larger **Audio buffer**
(30 or 40 ms) in the configuration; `RMT_AUDIO_DEBUG=1` shows the late audio
ticks and the underruns.

Issues of the Qt port are tracked on the [GitHub issue tracker of the port](https://github.com/gianlucarenzi/RITMO-Music-Tracker/issues).
Issues of the original RMT are tracked on the [upstream issue tracker](https://github.com/raster-atari-org/RASTER-Music-Tracker/issues).


### Credits

- [Radek Štěrba](http://atariki.krap.pl/index.php/Raster/C.P.U.), Raster/C.P.U., 2002-2009 ([original website](http://raster.infos.cz/atari/rmt/rmt.htm))<br>
  **R.I.P.** Thank you for everything you did, we truly miss you <3.
- Robert Petruzela, Bob!k/C.P.U. and - JirkaS/C.P.U.
- [Vin Samuel](https://github.com/VinsCool), VinsCool, 2021-2024
- [Peter Dell](https://www.wudsn.com), JAC!, 2024 to present
- [Gianluca Renzi](https://github.com/gianlucarenzi), 2026: the Qt port (Linux, Windows, macOS)

The logo and the icon (`doc/logo`, `src/res`) are drawn with the glyphs of the IBM VGA 9x16
font, "PxPlus IBM VGA 9x16" of [The Ultimate Oldschool PC Font Pack](https://int10h.org/oldschool-pc-fonts/)
by VileR (CC BY-SA 4.0).

#### Additional Credits
- New features, bugfixes and improvements for RMT 1.31-1.34 by VinsCool
- POKEY Tuning Calculations programming by VinsCool, with helpful advices from synthpopalooza and OPNA2608
- SAP-R Dumper and VUPlayer programming by VinsCool
- LZSS compression programming by DMSC, C++ port by VinsCool
- Unrolled LZSS music driver by Rensoupp, with few changes and new features by VinsCool
- New Bitmap graphics, ideas and beta testing by PG
- Ideas, features suggestions and inspiration by PG, Enderdude, Spring, Ivop, Tatqoo, Miker
- Spiteful inspiration by Rensoupp, Emkay, and anyone who challenged me to try doing things believed impossible or outside of my abilities ;)
- Special thanks to everyone from The Chiptune Café, AtariAge, and GBAtemp who motivated me to work harder on the revival of RMT!

### Greetings

- Fox/Taquart - Thanks for [XASM](https://github.com/pfusik/xasm) and [ASAP](https://asap.sourceforge.net)
- Jaskier/Taquart - Thanks for TMC and a lot of RMT routine speed/size optimizations
- Tatqoo/Taquart
- Sack/Cosine
- X-ray/Grayscale
- Greg/Grayscale
- Bewu/Grayscale
- PG - Thanks for the [ASMA - Atari SAP Music Archive](https://asma.atari.org)
- Fandal
- ZdenekB
- KrupkaJ
- Pepax
- LiSU
- Miker
- Dely
- Nils Feske
- Elan
- Wrathchild
- Kozyca
- Born/LaResistance
- Sal Esquivel
- Nooly
- All the active "Atariarea" Polish Atarians (https://atariarea.krap.pl)<br>
- ...and all other 8-bit Atarians all over the world! :-)


### Disclaimer

THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND.
AUTHOR DOES NOT WARRANT, GUARANTEE, OR MAKE ANY REPRESENTATIONS REGARDING THE USE, OR THE RESULTS OF USE, OF THE SOFTWARE OR WRITTEN MATERIALS IN TERMS OF CORRECTNESS, ACCURACY, RELIABILITY, CURRENTNESS, OR OTHERWISE.
THE ENTIRE RISK AS TO THE RESULTS AND PERFORMANCE OF THE SOFTWARE IS ASSUMED BY YOU.
