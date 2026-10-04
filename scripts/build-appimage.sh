#!/usr/bin/env bash
# scripts/build-appimage.sh - build RITMO (Qt6 frontend) as an AppImage
#
# For the architecture of the machine it runs on (uname -m):
# - x86_64: meant for Ubuntu 20.04 (glibc 2.31, the one of Debian 11), so that
#   the AppImage also runs on those systems and on every newer one;
# - aarch64 (Raspberry Pi 3/4/5 with a 64 bit system, ARM computers): meant
#   for Ubuntu 24.04, as the official Qt 6 for ARM64 needs glibc 2.38 (its
#   moc and libQt6Gui); the AppImage runs on Raspberry Pi OS 13 (trixie),
#   Debian 13, Ubuntu 24.04 and newer.
# Used by .github/workflows/build-linux.yml in those containers; locally:
#
#   docker run --rm -v "$PWD":/src -w /src ubuntu:20.04 scripts/build-appimage.sh
#
# Those Ubuntu versions have no Qt 6.8: the official Qt 6 is installed with
# aqtinstall and goes into the AppImage.
# GCC (10 on 20.04, the default one on 24.04), CMake (from pip on 20.04),
# PortAudio and RtMidi come from Ubuntu. linuxdeploy with its Qt
# plugin makes the AppImage; the ELF files in it may need no glibc newer
# than MAX_GLIBC.
#
# Environment: RMT_APPIMAGE_DEPS=0 skips installing the dependencies (they
# are there already), WORK is the build folder (default build-appimage/).
# Output: $WORK/Ritmo-Linux-<arch>.AppImage and $WORK/ritmo-linux.png, the
# offscreen screenshot of the smoke test run of the AppImage itself.

set -euo pipefail

QT_VERSION=6.8.3
ARCH=$(uname -m)
case $ARCH in
    x86_64)
        QT_HOST=linux QT_ARCH=linux_gcc_64 QT_SUBDIR=gcc_64
        MAX_GLIBC=2.31
        CC=gcc-10 CXX=g++-10 COMPILER_PACKAGES="gcc-10 g++-10"
        # the CMake of Ubuntu 20.04 is 3.16, RMT needs 3.25: from pip
        CMAKE_PACKAGES=""
        # the Python of aqtinstall: the 3.8 of Ubuntu 20.04 is too old (see below)
        PYTHON=python3.9 PYTHON_PACKAGES="python3.9 python3.9-venv python3.9-dev" ;;
    aarch64)
        QT_HOST=linux_arm64 QT_ARCH=linux_gcc_arm64 QT_SUBDIR=gcc_arm64
        MAX_GLIBC=2.39                     # Ubuntu 24.04: the official Qt 6.8 for ARM64 needs 2.38
        CC=gcc CXX=g++ COMPILER_PACKAGES="gcc g++"
        CMAKE_PACKAGES=cmake               # 3.28 (pip refuses the Python of the system there)
        PYTHON=python3 PYTHON_PACKAGES="python3-venv" ;;
    *)
        echo "No AppImage for $ARCH (x86_64 and aarch64 only)" >&2
        exit 2 ;;
esac
QT_DIR=/opt/qt/$QT_VERSION/$QT_SUBDIR

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$ROOT/build-appimage}
cd "$ROOT"

# --- dependencies ------------------------------------------------------------

if [ "${RMT_APPIMAGE_DEPS:-1}" = 1 ]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    # the libraries of the Qt xcb plugin, which linuxdeploy puts in the AppImage
    # (libxcb-cursor0: Qt 6.5+; libsm6 and the last ones come by themselves on
    # Ubuntu 20.04, not on 24.04), and those the CMake files of Qt6Gui look for
    apt-get install -y --no-install-recommends \
        $COMPILER_PACKAGES $CMAKE_PACKAGES ninja-build pkg-config git ca-certificates wget file \
        python3-pip python3-dev $PYTHON_PACKAGES imagemagick \
        portaudio19-dev librtmidi-dev libcups2 \
        libgl1-mesa-dev libegl1 libfontconfig1 libfreetype6 libdbus-1-3 \
        libxkbcommon-dev libxkbcommon-x11-0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
        libxcb-randr0 libxcb-render-util0 libxcb-xinerama0 libxcb-xfixes0 \
        libxcb-shape0 libxcb-xkb1 libxcb-util1 libxcb-cursor0 \
        libsm6 libice6 libx11-xcb1 libxcb-glx0 libxcb-shm0 libxcb-sync1 libxcb-render0
    if [ -z "$CMAKE_PACKAGES" ]; then
        python3 -m pip install --upgrade pip
        python3 -m pip install cmake
    fi
    # aqtinstall in a venv: the last one for the Python 3.8 of Ubuntu 20.04
    # (3.1.18) does not know the layout of the Qt 6.8 archives, so there it
    # is Python 3.9 (some of its modules are compiled: with gcc-10, the only
    # compiler there)
    $PYTHON -m venv /opt/aqt-venv
    /opt/aqt-venv/bin/python -m pip install --upgrade pip
    CC=$CC /opt/aqt-venv/bin/python -m pip install aqtinstall
    # (from /tmp: aqt writes aqtinstall.log in the current folder)
    [ -d "$QT_DIR" ] || (cd /tmp && /opt/aqt-venv/bin/python -m aqt install-qt $QT_HOST desktop $QT_VERSION $QT_ARCH -O /opt/qt)
fi

# --- build -------------------------------------------------------------------

git config --global --add safe.directory "$ROOT" 2>/dev/null || true
cmake -S . -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRMT_QT_MAJOR=6 \
    -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_PREFIX_PATH="$QT_DIR"
cmake --build "$WORK/build" --parallel

# --- AppDir ------------------------------------------------------------------

APPDIR=$WORK/AppDir
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
cp "$WORK/build/out/ritmo" "$APPDIR/usr/bin/"
cp -r "$WORK/build/out/resources" "$APPDIR/usr/bin/"     # RITMO finds them next to itself

cat > "$WORK/ritmo.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RITMO
Comment=Music tracker for the POKEY chip of the Atari XL/XE
Exec=ritmo %f
Icon=ritmo
Categories=AudioVideo;Audio;Music;
Terminal=false
DESKTOP
# the 256x256 icon of the program (src/res/ritmo-icon.png)
cp src/res/ritmo-icon.png "$WORK/ritmo.png"

# --- AppImage ----------------------------------------------------------------

cd "$WORK"
for tool in linuxdeploy-$ARCH.AppImage linuxdeploy-plugin-qt-$ARCH.AppImage; do
    [ -f $tool ] || wget -q https://github.com/linuxdeploy/${tool%-$ARCH.AppImage}/releases/download/continuous/$tool
    chmod +x $tool
done
export APPIMAGE_EXTRACT_AND_RUN=1           # no FUSE in containers
export QMAKE=$QT_DIR/bin/qmake
export LD_LIBRARY_PATH=$QT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so  # for the smoke test below
export OUTPUT=Ritmo-Linux-$ARCH.AppImage
rm -f $OUTPUT
./linuxdeploy-$ARCH.AppImage --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/ritmo" --desktop-file ritmo.desktop --icon-file ritmo.png \
    --plugin qt --output appimage

# --- checks ------------------------------------------------------------------

# The newest glibc symbol any ELF file of the AppImage needs
NEEDED=$(find "$APPDIR" -type f -exec sh -c 'file -b "$1" | grep -q ELF && objdump -T "$1"' _ {} \; 2>/dev/null \
    | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -1)
echo "glibc needed by the AppImage: $NEEDED (at most $MAX_GLIBC)"
if [ "$(printf '%s\n%s\n' "$NEEDED" "$MAX_GLIBC" | sort -V | tail -1)" != "$MAX_GLIBC" ]; then
    echo "The AppImage needs a glibc newer than $MAX_GLIBC"; exit 1
fi

# Smoke test of the AppImage itself: gemx.rmt, a screenshot after 3 s
rm -f ritmo-linux.png
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=$PWD/ritmo-linux.png RMT_QT_GRAB_MS=3000 XDG_CONFIG_HOME=$PWD/config \
    timeout 120 ./$OUTPUT "$ROOT/rmt/songs/rmt128/raster/gemx.rmt" || true
test -f ritmo-linux.png
ls -la $OUTPUT ritmo-linux.png
