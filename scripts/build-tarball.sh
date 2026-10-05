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
# The archive carries the libraries the program needs (Qt 6 with its plugins,
# PortAudio, RtMidi...) in lib/ and plugins/, found by the launcher script
# ./ritmo; it uses of the system only glibc and the graphics (OpenGL) and
# sound (ALSA) libraries, which belong to the driver of the machine. So it
# runs on the Debian release it was built on and on the newer ones (the glibc
# of the machine cannot be older than the one of the build); the README in it
# lists the few packages the machine needs.
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
    # qt6-qpa-plugins: the offscreen platform of the smoke test; qt6-wayland: the
    # Wayland platform plugin, carried in the archive
    apt-get install -y --no-install-recommends \
        g++ cmake ninja-build pkg-config git ca-certificates file \
        qt6-base-dev qt6-qpa-plugins qt6-wayland portaudio19-dev librtmidi-dev
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
mkdir -p "$DIR/lib" "$DIR/plugins"
cp -L "$WORK/build/out/ritmo" "$DIR/ritmo.bin"
cp -r "$WORK/build/out/resources" "$DIR/"     # RITMO finds them next to itself
cp LICENSE "$DIR/"

# The plugins of Qt that are loaded at run time: the platforms (X11, Wayland,
# offscreen), the image formats and the rest, from the folder of the Qt of the system
QT_PLUGINS=$(qtpaths6 --plugin-dir 2>/dev/null || true)
[ -d "$QT_PLUGINS" ] || QT_PLUGINS=$(dirname "$(find /usr/lib -type d -path '*/qt6/plugins/platforms' | head -1)")
for d in platforms imageformats iconengines xcbglintegrations platforminputcontexts \
         wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
    [ -d "$QT_PLUGINS/$d" ] && cp -rL "$QT_PLUGINS/$d" "$DIR/plugins/"
done

# What stays on the system: glibc, and the graphics and sound libraries that
# belong to the driver of the machine
SYSTEM_LIBS='^(ld-linux.*|ld64\.so.*|libc\.so.*|libm\.so.*|libdl\.so.*|libpthread\.so.*|librt\.so.*|libutil\.so.*|libresolv\.so.*|libnss_.*|libGL.*|libEGL.*|libOpenGL.*|libgbm.*|libdrm.*|libvulkan.*|libasound.*)$'

# The libraries of the program and of the plugins, and what they need in turn
# (ldd gives all of them)
ELFS="$DIR/ritmo.bin $(find "$DIR/plugins" -name '*.so')"
LIBS=$(for f in $ELFS; do ldd "$f" 2>/dev/null; done | awk '$2 == "=>" && $3 ~ /^\// {print $1 " " $3}' | sort -u)
echo "$LIBS" | while read -r name path; do
    if ! echo "$name" | grep -q -E "$SYSTEM_LIBS"; then
        cp -L "$path" "$DIR/lib/$name"
    fi
done

# The launcher: the program in the archive finds its libraries and plugins;
# the program folder (resources) is that of ritmo.bin
cat > "$DIR/ritmo" <<'LAUNCHER'
#!/bin/sh
HERE=$(dirname "$(readlink -f "$0")")
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/plugins"
exec "$HERE/ritmo.bin" "$@"
LAUNCHER
chmod +x "$DIR/ritmo"

# Nothing but the libraries of the system is left outside the archive
OUTSIDE=$(LD_LIBRARY_PATH=$DIR/lib ldd "$DIR/ritmo.bin" | awk '$2 == "=>" && $3 ~ /^\// {print $1 " " $3}' \
    | grep -v " $DIR/" | awk '{print $1}' | { grep -v -E "$SYSTEM_LIBS" || true; })
if [ -n "$OUTSIDE" ]; then
    echo "Libraries not in the archive: $OUTSIDE" >&2
    exit 1
fi

# The Debian packages of the libraries left to the system (glibc is there on
# every system)
PACKAGES=$(for lib in $(LD_LIBRARY_PATH=$DIR/lib ldd "$DIR/ritmo.bin" "$DIR"/plugins/*/*.so 2>/dev/null \
        | awk '$2 == "=>" && $3 ~ /^\// {print $1 " " $3}' | sort -u | grep -v " $DIR/" \
        | awk '{print $2}'); do
    dpkg -S "$(readlink -f "$lib")" 2>/dev/null || dpkg -S "$lib" 2>/dev/null || true
done | cut -d: -f1 | sort -u | { grep -v -E '^(libc6|libc6-dev)$' || true; })
# echo without quotes: the names on one line, one space between them
PACKAGES=$(echo $PACKAGES)
SYSTEM=$(. /etc/os-release && echo "$PRETTY_NAME")   # (in a subshell: os-release also sets NAME)

cat > "$DIR/README.txt" <<README
RITMO $(git describe --tags --always 2>/dev/null || true) for Linux $ARCH
================================================

Built on $SYSTEM. Qt 6, PortAudio and RtMidi are in this folder
(lib/ and plugins/); the machine needs glibc of that release or newer and
the graphics and sound libraries of its drivers:

    sudo apt install $PACKAGES

(the names of the packages may change in other distributions)

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
