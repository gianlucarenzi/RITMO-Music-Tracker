#!/usr/bin/env bash
# scripts/build-tarball.sh - build RITMO (Qt6 frontend) with the Qt 6 of
# Debian, as a .tar.gz for the architectures that have no AppImage (no
# official Qt and no linuxdeploy there): riscv64 and the like.
#
# Meant for a Debian container of that architecture; on GitHub
# (.github/workflows/build-linux-ports.yml) it runs emulated by QEMU:
#
#   docker run --rm --platform linux/riscv64 -v "$PWD":/src -w /src debian:trixie scripts/build-tarball.sh
#
# The program uses the libraries of the system, so the archive runs on the
# Debian release it was built on and on the newer ones; the README in it
# lists the packages to install, taken from the libraries the program needs.
#
# Environment: RMT_TARBALL_DEPS=0 skips installing the dependencies (they
# are there already), WORK is the build folder (default build-tarball/).
# Output: $WORK/Ritmo-Linux-<arch>.tar.gz and $WORK/ritmo-linux.png, the
# offscreen screenshot of the smoke test.

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$ROOT/build-tarball}
cd "$ROOT"

# --- dependencies ------------------------------------------------------------

if [ "${RMT_TARBALL_DEPS:-1}" = 1 ]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    # qt6-qpa-plugins: the offscreen platform of the smoke test
    apt-get install -y --no-install-recommends \
        g++ cmake ninja-build pkg-config git ca-certificates file \
        qt6-base-dev qt6-qpa-plugins portaudio19-dev librtmidi-dev
fi

# --- build -------------------------------------------------------------------

ARCH=$(dpkg --print-architecture)          # riscv64, ppc64, powerpc...
git config --global --add safe.directory "$ROOT" 2>/dev/null || true
cmake -S . -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRMT_QT_MAJOR=6
cmake --build "$WORK/build" --parallel

# --- archive -----------------------------------------------------------------

NAME=Ritmo-Linux-$ARCH
DIR=$WORK/$NAME
rm -rf "$DIR"
mkdir -p "$DIR"
cp -L "$WORK/build/out/ritmo" "$DIR/"
cp -r "$WORK/build/out/resources" "$DIR/"     # RITMO finds them next to itself
cp LICENSE "$DIR/"

# The Debian packages of the libraries the program is linked to (apt adds
# what they need in turn; not those of glibc and gcc, which every system has)
NEEDED=$(objdump -p "$DIR/ritmo" | awk '$1 == "NEEDED" {print $2}')
PACKAGES=$(for lib in $NEEDED; do
    path=$(ldd "$DIR/ritmo" | awk -v l="$lib" '$1 == l {print $3}')
    if [ -n "$path" ]; then
        dpkg -S "$(readlink -f "$path")" 2>/dev/null || dpkg -S "$path" 2>/dev/null || true
    fi
done | cut -d: -f1 | sort -u | { grep -v -E '^(libc6|libgcc-s1|libstdc\+\+6)$' || true; })
# the platform plugin of Qt for X11 and Wayland is loaded at run time, not linked;
# echo without quotes: the names on one line, one space between them
PACKAGES=$(echo $PACKAGES qt6-qpa-plugins qt6-wayland)
SYSTEM=$(. /etc/os-release && echo "$PRETTY_NAME")   # (in a subshell: os-release also sets NAME)

cat > "$DIR/README.txt" <<README
RITMO $(git describe --tags --always 2>/dev/null || true) for Linux $ARCH
================================================

Built on $SYSTEM with the Qt 6 of the distribution. It runs on that
release and on the newer ones that have these packages:

    sudo apt install $PACKAGES

Then start it from this folder (the resources folder must stay next to it):

    ./ritmo

The manual, the issues and the other downloads:
https://github.com/gianlucarenzi/RITMO-Music-Tracker
README

tar -C "$WORK" -czf "$WORK/$NAME.tar.gz" "$NAME"

# --- checks ------------------------------------------------------------------

# Smoke test of the program in the archive: gemx.rmt, a screenshot after 3 s
# (more time when emulated: the window may take long to come up)
rm -f "$WORK/ritmo-linux.png"
(cd "$WORK" && QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=$WORK/ritmo-linux.png RMT_QT_GRAB_MS=3000 \
    XDG_CONFIG_HOME=$WORK/config timeout 600 "$DIR/ritmo" "$ROOT/rmt/songs/rmt128/raster/gemx.rmt") || true
test -f "$WORK/ritmo-linux.png"
cat "$DIR/README.txt"
ls -la "$WORK/$NAME.tar.gz" "$WORK/ritmo-linux.png"
