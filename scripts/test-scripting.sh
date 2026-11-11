#!/bin/bash
# Runs the reference scripts of test-resources/scripts through "ritmo /SCRIPT:",
# twice each into separate folders: the exit code must be 0, the expected files
# must exist and both runs must produce byte-identical files (WAV included).
#
#   scripts/test-scripting.sh [path/to/ritmo]      (default: build-qt/out/ritmo)
set -u
cd "$(dirname "$0")/.."
RMT=${1:-build-qt/out/ritmo}
[ -x "$RMT" ] || { echo "No such program: $RMT" >&2; exit 2; }
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
# the settings (tuning, driver...) are the defaults, not those of the user running the test
export XDG_CONFIG_HOME="$WORK/config"
fail=0

run() { # script output-folder
    RMT_SCRIPT_OUTPUT="$2" "$RMT" "/SCRIPT:test-resources/scripts/$1.rmtscript" >"$2.log" 2>&1
}

check() { # name expected-files...
    local name=$1; shift
    run "$name" "$WORK/$name-a" || { echo "FAIL $name: exit code $?"; tail -5 "$WORK/$name-a.log"; fail=1; return; }
    run "$name" "$WORK/$name-b" || { echo "FAIL $name (second run): exit code $?"; fail=1; return; }
    for f in "$@"; do
        [ -s "$WORK/$name-a/$f" ] || { echo "FAIL $name: $f is missing or empty"; fail=1; }
    done
    if diff -r "$WORK/$name-a" "$WORK/$name-b" >/dev/null; then
        echo "ok   $name: $(ls "$WORK/$name-a" | wc -l) files, both runs identical"
    else
        echo "FAIL $name: the two runs differ"; diff -rq "$WORK/$name-a" "$WORK/$name-b"; fail=1
    fi
}

check delta delta.rmt delta.txt delta.rmw d1.rmt d1.asm d1.sapr d1.lzss d1.sap d1.xex d1_rmt.xex d1_player.asm d1.wav d2.sap d2.xex d2_rmt.xex d3.sapr d3_rmt.xex d4_rmt.xex
check midi midi.rmt midi.txt midi.rmw
check dump actions.md notekeys.md
check stereo stereo.rmt stereo.txt stereo.rmw s1.rmt s1.asm s1.sapr s1.lzss s1_player.asm s1.wav s1_rmt.xex s2_rmt.xex s2.rmt s2.asm s2_player.asm short.sap short2.sap short.xex short.wav short.sapr

# the SAP type B files: the blocks must hold the player (an earlier export left the driver out)
if command -v python3 >/dev/null; then
    for f in d1.sap d2.sap; do
        scripts/check-sap.py "$WORK/delta-a/$f" 1 || fail=1
    done
    scripts/check-sap.py "$WORK/stereo-a/short.sap" 1 || fail=1
    scripts/check-sap.py "$WORK/stereo-a/short2.sap" 2 || fail=1
    # the XEX with the RMT player: the player of the driver, the module after it, the run address and the NAME line
    # (default: the song name up to its first ", "; Delta.rmt has no name: the file it was saved as, delta.rmt)
    scripts/check-xex-rmt.py "$WORK/delta-a/d1_rmt.xex" 4 "delta" || fail=1
    scripts/check-xex-rmt.py "$WORK/delta-a/d2_rmt.xex" 4 "Name Two" || fail=1
    scripts/check-xex-rmt.py "$WORK/delta-a/d3_rmt.xex" 4 "NTSC" || fail=1
    scripts/check-xex-rmt.py "$WORK/delta-a/d4_rmt.xex" 4 "RMT 1.28" || fail=1
    scripts/check-xex-rmt.py "$WORK/stereo-a/s1_rmt.xex" 8 "STEREO" || fail=1
    scripts/check-xex-rmt.py "$WORK/stereo-a/s2_rmt.xex" 8 "STEREO 1.28" || fail=1
    # the WAV lasts as long as the SAP-R stream (the stereo song is at instrument speed 4)
    scripts/check-wav.py "$WORK/delta-a/d1.sapr" "$WORK/delta-a/d1.wav" || fail=1
    scripts/check-wav.py "$WORK/stereo-a/s1.sapr" "$WORK/stereo-a/s1.wav" || fail=1
else
    echo "skip the SAP and WAV structure checks: no python3"
fi

# the stereo song that does not fit the memory of the LZSS player: refused with the sizes, no file left
"$RMT" "/SCRIPT:test-resources/scripts/stereo-too-big.rmtscript" >"$WORK/toobig.log" 2>&1; code=$?
if [ $code -eq 1 ] && grep -q "too big to fit in memory" "$WORK/toobig.log" && ! [ -e test-resources/scripts/out/stereo-too-big/too-big.sap ]; then
    echo "ok   stereo song too big for the LZSS player: refused, exit code 1"
else
    echo "FAIL stereo song too big for the LZSS player: exit code $code"; tail -3 "$WORK/toobig.log"; fail=1
fi

# the XEX with the RMT player and a driver it has no player for: refused with a message, no file left
printf 'set output %s\nopen "%s/test-resources/songs/Delta.rmt"\nset driver patch8\nexport xex-rmt nodriver.xex\n' "$WORK/nodriver" "$PWD" >"$WORK/nodriver.rmtscript"
"$RMT" "/SCRIPT:$WORK/nodriver.rmtscript" >"$WORK/nodriver.log" 2>&1; code=$?
if [ $code -eq 1 ] && grep -q "There is an RMT player for the drivers" "$WORK/nodriver.log" && ! [ -e "$WORK/nodriver/nodriver.xex" ]; then
    echo "ok   XEX with the RMT player and the Patch8 driver: refused, exit code 1"
else
    echo "FAIL XEX with the RMT player and the Patch8 driver: exit code $code"; tail -3 "$WORK/nodriver.log"; fail=1
fi

# a WAV export with a driver of the v1 family leaves the driver as it was: the frequency export after it reads the
# same tables (the WAV export used to write into the Patch16 driver's variables, across the v1 driver's tables)
printf 'set output %s\nopen "%s/test-resources/songs/Delta.rmt"\nset driver unpatched\nexport asm before.asm type=song notes=freq\nexport wav v1.wav\nexport asm after.asm type=song notes=freq\n' "$WORK/v1wav" "$PWD" >"$WORK/v1wav.rmtscript"
"$RMT" "/SCRIPT:$WORK/v1wav.rmtscript" >"$WORK/v1wav.log" 2>&1; code=$?
if [ $code -eq 0 ] && cmp -s "$WORK/v1wav/before.asm" "$WORK/v1wav/after.asm"; then
    echo "ok   WAV export with the Unpatched driver: the driver's tables intact"
else
    echo "FAIL WAV export with the Unpatched driver: exit code $code"; tail -3 "$WORK/v1wav.log"; fail=1
fi

# a driver file that is missing: "set driver" fails with the file's name instead of exporting another driver's sound
# (a copy of the program without rmt_driver_v1.obx)
cp -rL "$(dirname "$RMT")" "$WORK/nodriverfile" && rm "$WORK/nodriverfile/resources/drivers/rmt_driver_v1.obx"
printf 'set output %s\nopen "%s/test-resources/songs/Delta.rmt"\nset driver unpatched\nexport wav missing.wav\n' "$WORK/nodriverfile-out" "$PWD" >"$WORK/nodriverfile.rmtscript"
"$WORK/nodriverfile/$(basename "$RMT")" "/SCRIPT:$WORK/nodriverfile.rmtscript" >"$WORK/nodriverfile.log" 2>&1; code=$?
if [ $code -eq 1 ] && grep -q "rmt_driver_v1.obx" "$WORK/nodriverfile.log" && ! [ -e "$WORK/nodriverfile-out/missing.wav" ]; then
    echo "ok   missing driver file: named, exit code 1, no WAV"
else
    echo "FAIL missing driver file: exit code $code"; tail -3 "$WORK/nodriverfile.log"; fail=1
fi

# issue #5: "stripped-rmt" with sfx=yes keeps the instruments of an SFX-only module (no tracks): at its own
# address $6800 the export is the module byte for byte, relocated it keeps its length; sfx=no strips it to 30 bytes
SFX=test-resources/songs/issue_5_stripped_sfx_export/testsfx.rmt
printf 'set output %s\nopen "%s/%s"\nexport stripped-rmt same.rmt address=$6800 sfx=yes\nexport stripped-rmt moved.rmt address=$6400 sfx=yes\nexport stripped-rmt nosfx.rmt address=$6400 sfx=no\n' \
    "$WORK/sfx" "$PWD" "$SFX" >"$WORK/sfx.rmtscript"
"$RMT" "/SCRIPT:$WORK/sfx.rmtscript" >"$WORK/sfx.log" 2>&1; code=$?
if [ $code -eq 0 ] && cmp -s "$SFX" "$WORK/sfx/same.rmt" && [ "$(stat -c %s "$WORK/sfx/moved.rmt")" -eq "$(stat -c %s "$SFX")" ] \
    && [ "$(od -An -tx1 -j2 -N2 "$WORK/sfx/moved.rmt" | tr -d ' ')" = "0064" ] && [ "$(stat -c %s "$WORK/sfx/nosfx.rmt")" -eq 30 ]; then
    echo "ok   stripped RMT of an SFX-only module: sfx=yes keeps the instruments, sfx=no strips them"
else
    echo "FAIL stripped RMT of an SFX-only module: exit code $code"; tail -3 "$WORK/sfx.log"; ls -l "$WORK/sfx" 2>/dev/null; fail=1
fi

# tuning.ini (RMT 1.36, plans 59 and 62): a script run reads it from RMT_CONFIG_DIR. The tuning of an older file
# (TUNING, BASENOTE and the ratios as fractions) is read as its conversion (A4_FREQUENCY, TONIC and cents): the same
# frequency tables, so the same ASM export with frequencies; it differs from the default tuning; a retired preset
# (5, Tempérament Égal à Quintes Justes) plays as equal temperament
tuningrun() { # name tuning.ini-text
    mkdir -p "$WORK/tuning-$1"
    printf "$2" >"$WORK/tuning-$1/tuning.ini"
    printf 'set output %s\nopen "%s/test-resources/songs/Delta.rmt"\nexport asm freq.asm type=song notes=freq\n' "$WORK/tuning-$1-out" "$PWD" >"$WORK/tuning-$1.rmtscript"
    RMT_CONFIG_DIR="$WORK/tuning-$1" "$RMT" "/SCRIPT:$WORK/tuning-$1.rmtscript" >"$WORK/tuning-$1.log" 2>&1
}
tuningrun old 'TUNING = 432\nBASENOTE = 0\nTEMPERAMENT = 29\nUNISON = 1 / 1\nMIN_2ND = 16 / 15\nMAJ_2ND = 9 / 8\nMIN_3RD = 6 / 5\nMAJ_3RD = 5 / 4\nPERF_4TH = 4 / 3\nTRITONE = 45 / 32\nPERF_5TH = 3 / 2\nMIN_6TH = 8 / 5\nMAJ_6TH = 5 / 3\nMIN_7TH = 9 / 5\nMAJ_7TH = 15 / 8\nOCTAVE = 2 / 1\n'
tuningrun new 'A4_FREQUENCY = 432\nTONIC = 0\nTEMPERAMENT = 29\nMIN_2ND = 111.73128526977774\nMAJ_2ND = 203.91000173077484\nMIN_3RD = 315.64128700055255\nMAJ_3RD = 386.3137138648348\nPERF_4TH = 498.0449991346125\nTRITONE = 590.2237155956096\nPERF_5TH = 701.9550008653874\nMIN_6TH = 813.6862861351652\nMAJ_6TH = 884.3587129994474\nMIN_7TH = 1017.5962878659401\nMAJ_7TH = 1088.2687147302222\n'
tuningrun default 'A4_FREQUENCY = 440.83751645933\nTONIC = 3\nTEMPERAMENT = 0\n'
tuningrun retired 'A4_FREQUENCY = 440.83751645933\nTONIC = 3\nTEMPERAMENT = 5\n'
if cmp -s "$WORK/tuning-old-out/freq.asm" "$WORK/tuning-new-out/freq.asm" && [ -s "$WORK/tuning-old-out/freq.asm" ] \
    && ! cmp -s "$WORK/tuning-new-out/freq.asm" "$WORK/tuning-default-out/freq.asm" \
    && cmp -s "$WORK/tuning-default-out/freq.asm" "$WORK/tuning-retired-out/freq.asm"; then
    echo "ok   tuning.ini: the old names and fractions read as A4_FREQUENCY, TONIC and cents; a retired preset is equal"
else
    echo "FAIL tuning.ini of the old and the new format"; ls -l "$WORK"/tuning-*-out/ 2>/dev/null; fail=1
fi

# the relocatable ASM in the ca65 format (RITMO issue #5): assembled by ca65 and linked at $4000 it is the stripped
# module at $4000, byte for byte (skipped without cc65)
if command -v ca65 >/dev/null && command -v ld65 >/dev/null; then
    printf 'set output %s\nopen "%s/test-resources/songs/Delta.rmt"\nexport stripped-rmt ref.rmt address=$4000 sfx=yes\nexport rmtplayer-asm song.asm relocate=none asmformat=ca65 sfx=yes\n' \
        "$WORK/ca65" "$PWD" >"$WORK/ca65.rmtscript"
    printf 'MEMORY { RAM: start = $4000, size = $C000, file = %%O; }\nSEGMENTS { CODE: load = RAM, type = ro; }\n' >"$WORK/ca65.cfg"
    "$RMT" "/SCRIPT:$WORK/ca65.rmtscript" >"$WORK/ca65.log" 2>&1; code=$?
    if [ $code -eq 0 ] && ca65 "$WORK/ca65/song.asm" -o "$WORK/ca65/song.o" >>"$WORK/ca65.log" 2>&1 \
        && ld65 -C "$WORK/ca65.cfg" "$WORK/ca65/song.o" -o "$WORK/ca65/song.bin" >>"$WORK/ca65.log" 2>&1 \
        && tail -c +7 "$WORK/ca65/ref.rmt" | cmp -s - "$WORK/ca65/song.bin"; then
        echo "ok   relocatable ASM for ca65: assembled, it is the stripped module byte for byte"
    else
        echo "FAIL relocatable ASM for ca65: exit code $code"; tail -5 "$WORK/ca65.log"; fail=1
    fi
else
    echo "skip relocatable ASM for ca65: no cc65"
fi

# RMT_CONFIG_DIR: the configuration of that folder is read (an NTSC ritmo.ini makes an NTSC SAP-R) and not written
mkdir -p "$WORK/configdir"
printf 'NTSC_SYSTEM = 1\n' >"$WORK/configdir/ritmo.ini"
printf 'set output %s\nopen "%s/test-resources/songs/Delta.rmt"\nexport sapr ntsc.sapr date="01/01/2026"\n' "$WORK/configdir-out" "$PWD" >"$WORK/configdir.rmtscript"
RMT_CONFIG_DIR="$WORK/configdir" "$RMT" "/SCRIPT:$WORK/configdir.rmtscript" >"$WORK/configdir.log" 2>&1; code=$?
if [ $code -eq 0 ] && head -c 200 "$WORK/configdir-out/ntsc.sapr" | grep -aq "^NTSC" && [ "$(ls "$WORK/configdir")" = "ritmo.ini" ] \
    && [ "$(cat "$WORK/configdir/ritmo.ini")" = "NTSC_SYSTEM = 1" ]; then
    echo "ok   RMT_CONFIG_DIR: its ritmo.ini read (NTSC SAP-R), nothing written"
else
    echo "FAIL RMT_CONFIG_DIR: exit code $code"; tail -3 "$WORK/configdir.log"; fail=1
fi

# the failure paths: a syntax error is exit code 2, a failing command 1
printf 'open "unterminated\n' >"$WORK/syntax.rmtscript"
"$RMT" "/SCRIPT:$WORK/syntax.rmtscript" >/dev/null 2>&1; code=$?
[ $code -eq 2 ] && echo "ok   syntax error: exit code 2" || { echo "FAIL syntax error: exit code $code, expected 2"; fail=1; }
printf 'open nothere.rmt\n' >"$WORK/missing.rmtscript"
"$RMT" "/SCRIPT:$WORK/missing.rmtscript" >/dev/null 2>&1; code=$?
[ $code -eq 1 ] && echo "ok   failing command: exit code 1" || { echo "FAIL failing command: exit code $code, expected 1"; fail=1; }

# a script run reads the configuration and never writes it, not even the defaults on a machine that has none
if [ -z "$(find "$XDG_CONFIG_HOME" -type f 2>/dev/null)" ]; then
    echo "ok   no configuration written by the script runs"
else
    echo "FAIL the script runs wrote a configuration:"; find "$XDG_CONFIG_HOME" -type f; fail=1
fi

exit $fail
