#!/usr/bin/env bash
# scripts/build-appimage-ports.sh - the folder of scripts/build-tarball.sh as
# an AppImage, for the architectures that have no official appimagetool,
# runtime or linuxdeploy: riscv64, ppc64 (big endian) and ppc64le
#
#   scripts/build-appimage-ports.sh appdir        # in the container of the architecture
#   scripts/build-appimage-ports.sh pack <arch>   # on the x86_64 machine (no emulation)
#   scripts/build-appimage-ports.sh test          # in the container of the architecture
#
# appdir runs after scripts/build-tarball.sh, in the same Debian container
# (emulated by QEMU on GitHub, .github/workflows/build-linux-ports.yml): its
# folder $WORK/Ritmo-Linux-<debian arch> already carries Qt 6, PortAudio and
# RtMidi and the launcher ./ritmo, so it only needs AppRun, the .desktop file
# and the icon to be an AppDir ($WORK/AppDir).
# pack makes $WORK/Ritmo-<version>-<arch>.AppImage and its .zsync with
# appimagetool for x86_64 and the runtime of the architecture, from the ports
# in gianlucarenzi/appimagetool (release ports-2) and gianlucarenzi/type2-runtime
# (release ports-1), checked against the sha256 sums below: appimagetool does
# not need to run on the architecture it packs for, so no emulation there.
# test runs the AppImage offscreen: $WORK/ritmo-appimage.png.
#
# Environment: WORK is the build folder (default build-tarball/, the one of
# build-tarball.sh); RMT_APPIMAGE_DEPS=0 skips installing the dependencies of test.

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$ROOT/build-tarball}

# The tools of the ports: a fixed release, so that a release of RITMO does not
# change when they are built again. When appimagetool and type2-runtime of
# AppImage have these architectures, their continuous releases take over.
APPIMAGETOOL_URL=https://github.com/gianlucarenzi/appimagetool/releases/download/ports-2/appimagetool-x86_64.AppImage
RUNTIME_URL_BASE=https://github.com/gianlucarenzi/type2-runtime/releases/download/ports-1
declare -A SHA256=(
    [appimagetool-x86_64.AppImage]=a87b72efe8d4e4e952866ffae56a05714e19cdf5cb2d51e1076aa669873fe405
    [runtime-riscv64]=8a6daf401dd8ee4b438bd90a27cba13e21cbe172be126003dd969143d459390b
    [runtime-ppc64]=d88b7ec8ca4189a2c2ac2cd5a1db257b8fd49f002d0b06b9429467eb49f5653d
    [runtime-ppc64le]=e0ee28e9b9d18e3c81590c169600945cfc73f50e82586f0354a59d3c348d437a
)

# download URL FILE: once, and only if its sha256 is the expected one
download() {
    [ -f "$2" ] || wget -q -O "$2.part" "$1"
    [ -f "$2" ] || mv "$2.part" "$2"
    echo "${SHA256[$2]}  $2" | sha256sum -c -
}

# Emulated by QEMU (qemu-user with binfmt_misc), an AppImage does not start:
# the rule of QEMU wants zeros in bytes 8-15 of the ELF header, where the
# AppImage has its magic bytes AI\x02 ("Exec format error"). A copy without
# them runs the same runtime and the same contents (the runtime does not
# read them). On a real machine the AppImage itself runs.
runnable() {
    if "./$1" --appimage-version >/dev/null 2>&1; then
        echo "./$1"
    else
        cp "$1" "$1.qemu"
        printf '\0\0\0' | dd of="$1.qemu" bs=1 seek=8 count=3 conv=notrunc 2>/dev/null
        echo "./$1.qemu"
    fi
}

case "${1:-}" in
appdir)
    DIR=$WORK/Ritmo-Linux-$(dpkg --print-architecture)
    [ -x "$DIR/ritmo" ] || { echo "No $DIR/ritmo: run scripts/build-tarball.sh first" >&2; exit 1; }
    APPDIR=$WORK/AppDir
    rm -rf "$APPDIR"
    cp -a "$DIR" "$APPDIR"
    # the launcher of the archive finds lib/ and plugins/ next to itself
    mv "$APPDIR/ritmo" "$APPDIR/AppRun"
    cat > "$APPDIR/ritmo.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RITMO
Comment=Music tracker for the POKEY chip of the Atari XL/XE
Exec=ritmo %f
Icon=ritmo
Categories=AudioVideo;Audio;Music;
Terminal=false
DESKTOP
    cp "$ROOT/src/res/ritmo-icon.png" "$APPDIR/ritmo.png"
    ln -sf ritmo.png "$APPDIR/.DirIcon"
    ;;

pack)
    ARCH=${2:?give the architecture: riscv64, ppc64 or ppc64le}
    [ -d "$WORK/AppDir" ] || { echo "No $WORK/AppDir: run $0 appdir first" >&2; exit 1; }
    cd "$WORK"
    download "$APPIMAGETOOL_URL" appimagetool-x86_64.AppImage
    download "$RUNTIME_URL_BASE/runtime-$ARCH" runtime-$ARCH
    chmod +x appimagetool-x86_64.AppImage
    OUTPUT=Ritmo-$("$ROOT/scripts/package-version.sh")-$ARCH.AppImage
    rm -f Ritmo-*-$ARCH.AppImage Ritmo-*-$ARCH.AppImage.zsync
    APPIMAGE_EXTRACT_AND_RUN=1 ARCH=$ARCH ./appimagetool-x86_64.AppImage --runtime-file runtime-$ARCH \
        -u "gh-releases-zsync|gianlucarenzi|RITMO-Music-Tracker|latest|Ritmo-*-$ARCH.AppImage.zsync" \
        AppDir "$OUTPUT"
    file "$OUTPUT"
    test -f "$OUTPUT.zsync"
    ;;

test)
    if [ "${RMT_APPIMAGE_DEPS:-1}" = 1 ]; then
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        # what the AppImage leaves to the system (glibc, the graphics and sound
        # libraries of the drivers), as on the machine of a user: the packages
        # that README.txt of the .tar.gz lists ("sudo apt install ...")
        apt-get install -y --no-install-recommends file \
            $(sed -n 's/^ *sudo apt install //p' "$WORK/AppDir/README.txt")
    fi
    cd "$WORK"
    OUTPUT=$(ls Ritmo-*-"$(uname -m)".AppImage)
    file "$OUTPUT"
    # Smoke test of the AppImage itself: gemx.rmt, a screenshot after 3 s
    # (more time when emulated: the window may take long to come up)
    rm -f ritmo-appimage.png "$OUTPUT.qemu"
    APPIMAGE_EXTRACT_AND_RUN=1 QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=$PWD/ritmo-appimage.png RMT_QT_GRAB_MS=3000 \
        XDG_CONFIG_HOME=$PWD/config timeout 600 $(runnable "$OUTPUT") "$ROOT/rmt/songs/rmt128/raster/gemx.rmt" || true
    rm -f "$OUTPUT.qemu"
    test -f ritmo-appimage.png
    ls -la "$OUTPUT" "$OUTPUT.zsync" ritmo-appimage.png
    ;;

*)
    echo "Usage: $0 appdir | pack <arch> | test" >&2
    exit 2 ;;
esac
