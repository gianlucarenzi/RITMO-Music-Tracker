#!/usr/bin/env bash
# scripts/test-endian.sh - the engine of RITMO on another architecture gives
# the same results as on this one: the songs of rmt/songs played by two
# RmtCoreTest (one native, one usually run by QEMU) must write the same POKEY
# registers frame by frame, the same sound and the same screen.
#
# Meant for the big endian machines (PowerPC), where every value read or
# written as more than one byte shows if it depends on the byte order:
#
#   scripts/test-endian.sh build-core-linux/out/RmtCoreTest \
#       "qemu-ppc64 -L /usr/powerpc64-linux-gnu build-ppc64/out/RmtCoreTest"
#
# Environment: FRAMES (default 500) frames played per song, SONGS the files,
# one per line (default all of rmt/songs; the names may have spaces), WORK
# the folder of the results.
# Exit code 1 when a song differs; the first different line is printed.

set -uo pipefail

NATIVE=${1:?the native RmtCoreTest}
OTHER=${2:?the command of the other RmtCoreTest}
FRAMES=${FRAMES:-500}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$(mktemp -d)}
mkdir -p "$WORK/native" "$WORK/other"
cd "$ROOT"
[ -n "${SONGS:-}" ] || SONGS=$(find rmt/songs -name '*.rmt' | sort)
mapfile -t SONG_LIST <<<"$SONGS"

failed=0
total=0
for song in "${SONG_LIST[@]}"; do
    total=$((total + 1))
    id=$(echo "$song" | tr '/' '_')
    $NATIVE --play "$song" "$FRAMES" "$WORK/native/$id.regs" "$WORK/native/$id.wav" >/dev/null 2>&1
    $OTHER --play "$song" "$FRAMES" "$WORK/other/$id.regs" "$WORK/other/$id.wav" >/dev/null 2>&1
    if ! cmp -s "$WORK/native/$id.regs" "$WORK/other/$id.regs"; then
        echo "FAIL $song: POKEY registers"
        diff "$WORK/native/$id.regs" "$WORK/other/$id.regs" | head -3
        failed=$((failed + 1))
    elif ! cmp -s "$WORK/native/$id.wav" "$WORK/other/$id.wav"; then
        echo "FAIL $song: sound ($(cmp "$WORK/native/$id.wav" "$WORK/other/$id.wav" | head -1))"
        failed=$((failed + 1))
    fi
done

# the screen of a mono and of a stereo song
for song in rmt/songs/rmt128/raster/gemx.rmt rmt/songs/rmt128/others/hightide.rmt; do
    total=$((total + 1))
    id=$(echo "$song" | tr '/' '_')
    $NATIVE --screenshot "$WORK/native/$id.ppm" "$song" >/dev/null 2>&1
    $OTHER --screenshot "$WORK/other/$id.ppm" "$song" >/dev/null 2>&1
    if ! cmp -s "$WORK/native/$id.ppm" "$WORK/other/$id.ppm"; then
        echo "FAIL $song: screen"
        failed=$((failed + 1))
    fi
done

echo "$((total - failed)) of $total the same (results in $WORK)"
[ "$failed" = 0 ]
