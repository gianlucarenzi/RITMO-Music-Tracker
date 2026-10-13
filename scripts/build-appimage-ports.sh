#!/usr/bin/env bash
# scripts/build-appimage-ports.sh - the folder of scripts/build-tarball.sh as
# an AppImage, for the architectures that have no official appimagetool and
# no linuxdeploy (riscv64 for now)
#
# Runs after scripts/build-tarball.sh, in the same Debian container of the
# architecture (emulated by QEMU on GitHub, .github/workflows/build-linux-ports.yml):
# its folder $WORK/Ritmo-Linux-<arch> already carries Qt 6, PortAudio and
# RtMidi and the launcher ./ritmo, so it only needs AppRun, the .desktop file
# and the icon to be an AppDir. appimagetool and the runtime of that
# architecture come from the riscv64 ports of gianlucarenzi/appimagetool and
# gianlucarenzi/type2-runtime (APPIMAGETOOL_URL and RUNTIME_URL change them).
#
# Environment: RMT_APPIMAGE_DEPS=0 skips installing the dependencies, WORK is
# the build folder (default build-tarball/, the one of build-tarball.sh).
# Output: $WORK/Ritmo-<version>-<arch>.AppImage, its .zsync for AppImageUpdate,
# and $WORK/ritmo-appimage.png, the offscreen screenshot of the smoke test
# run of the AppImage itself.

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$ROOT/build-tarball}
ARCH=$(uname -m)                            # riscv64 (the names of AppImage, not of Debian)
APPIMAGETOOL_URL=${APPIMAGETOOL_URL:-https://github.com/gianlucarenzi/appimagetool/releases/download/riscv64-continuous/appimagetool-$ARCH.AppImage}
RUNTIME_URL=${RUNTIME_URL:-https://github.com/gianlucarenzi/type2-runtime/releases/download/riscv64-continuous/runtime-$ARCH}

DIR=$WORK/Ritmo-Linux-$(dpkg --print-architecture)
[ -x "$DIR/ritmo" ] || { echo "No $DIR/ritmo: run scripts/build-tarball.sh first" >&2; exit 1; }

if [ "${RMT_APPIMAGE_DEPS:-1}" = 1 ]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends wget ca-certificates file
fi

# --- AppDir ------------------------------------------------------------------

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

# --- AppImage ----------------------------------------------------------------

cd "$WORK"
[ -f appimagetool-$ARCH.AppImage ] || wget -q -O appimagetool-$ARCH.AppImage "$APPIMAGETOOL_URL"
[ -f runtime-$ARCH ] || wget -q -O runtime-$ARCH "$RUNTIME_URL"
chmod +x appimagetool-$ARCH.AppImage
export APPIMAGE_EXTRACT_AND_RUN=1           # no FUSE in containers

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

OUTPUT=Ritmo-$("$ROOT/scripts/package-version.sh")-$ARCH.AppImage
rm -f Ritmo-*-$ARCH.AppImage Ritmo-*-$ARCH.AppImage.zsync Ritmo-*-$ARCH.AppImage.qemu
APPIMAGETOOL=$(runnable appimagetool-$ARCH.AppImage)
ARCH=$ARCH $APPIMAGETOOL --runtime-file runtime-$ARCH \
    -u "gh-releases-zsync|gianlucarenzi|RITMO-Music-Tracker|latest|Ritmo-*-$ARCH.AppImage.zsync" \
    "$APPDIR" "$OUTPUT"

# --- checks ------------------------------------------------------------------

file "$OUTPUT"
test -f "$OUTPUT.zsync"

# Smoke test of the AppImage itself: gemx.rmt, a screenshot after 3 s
# (more time when emulated: the window may take long to come up)
rm -f ritmo-appimage.png
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=$PWD/ritmo-appimage.png RMT_QT_GRAB_MS=3000 XDG_CONFIG_HOME=$PWD/config \
    timeout 600 $(runnable $OUTPUT) "$ROOT/rmt/songs/rmt128/raster/gemx.rmt" || true
rm -f $OUTPUT.qemu
test -f ritmo-appimage.png
ls -la $OUTPUT $OUTPUT.zsync ritmo-appimage.png
