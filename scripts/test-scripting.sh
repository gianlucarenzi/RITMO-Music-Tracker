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

# the failure paths: a syntax error is exit code 2, a failing command 1
printf 'open "unterminated\n' >"$WORK/syntax.rmtscript"
"$RMT" "/SCRIPT:$WORK/syntax.rmtscript" >/dev/null 2>&1; code=$?
[ $code -eq 2 ] && echo "ok   syntax error: exit code 2" || { echo "FAIL syntax error: exit code $code, expected 2"; fail=1; }
printf 'open nothere.rmt\n' >"$WORK/missing.rmtscript"
"$RMT" "/SCRIPT:$WORK/missing.rmtscript" >/dev/null 2>&1; code=$?
[ $code -eq 1 ] && echo "ok   failing command: exit code 1" || { echo "FAIL failing command: exit code $code, expected 1"; fail=1; }

exit $fail
