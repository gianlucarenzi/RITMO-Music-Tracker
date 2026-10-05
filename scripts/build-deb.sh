#!/usr/bin/env bash
# scripts/build-deb.sh - the .deb of RITMO (Qt6 frontend) for the Debian
# architectures that have no AppImage (riscv64, ppc64...): the program built
# by scripts/build-tarball.sh, with the libraries it needs as the Depends of
# the package (dpkg-shlibdeps), so that
#
#   sudo apt install ./ritmo_2.5_riscv64.deb
#
# installs Qt 6, PortAudio, RtMidi and the rest by itself. It runs in the same
# Debian container, after build-tarball.sh (it needs its build folder):
#
#   docker run --rm --platform linux/riscv64 -v "$PWD":/src -w /src debian:trixie \
#       sh -c 'scripts/build-tarball.sh && scripts/build-deb.sh'
#
# The Depends are those of the release of the build, so the package installs on
# that release and on the newer ones.
#
# Environment: WORK is the build folder of build-tarball.sh (default
# build-tarball/). Output: $WORK/ritmo_<version>_<arch>.deb

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$ROOT/build-tarball}
cd "$ROOT"

ARCH=$(dpkg --print-architecture)
# v2.5 -> 2.5, v2.5-rc2 -> 2.5~rc2 (older than 2.5), v2.5-rc2-5-g43f47b8 -> 2.5~rc2.5.g43f47b8
DESCRIBE=$(git describe --tags --always 2>/dev/null || echo 0)
VERSION=${DESCRIBE#v}
VERSION=${VERSION//-rc/~rc}
VERSION=${VERSION//-/.}
[[ $VERSION =~ ^[0-9] ]] || VERSION=0~$VERSION

PKG=$WORK/deb
rm -rf "$PKG"
mkdir -p "$PKG/DEBIAN" "$PKG/usr/lib/ritmo" "$PKG/usr/bin" "$PKG/usr/share/applications" \
    "$PKG/usr/share/icons/hicolor/256x256/apps" "$PKG/usr/share/doc/ritmo"

# The program and its resources in the same folder (RITMO finds them next to
# the executable, also through the link of /usr/bin)
cp -L "$WORK/build/out/ritmo" "$PKG/usr/lib/ritmo/ritmo"
cp -r "$WORK/build/out/resources" "$PKG/usr/lib/ritmo/"
ln -s ../lib/ritmo/ritmo "$PKG/usr/bin/ritmo"
cp src/res/ritmo-icon.png "$PKG/usr/share/icons/hicolor/256x256/apps/ritmo.png"
cp LICENSE "$PKG/usr/share/doc/ritmo/copyright"
cat > "$PKG/usr/share/applications/ritmo.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RITMO
Comment=Music tracker for the POKEY chip of the Atari XL/XE
Exec=ritmo %f
Icon=ritmo
Categories=AudioVideo;Audio;Music;
Terminal=false
DESKTOP

# The libraries the program is linked to, as Depends; the platform plugins of
# Qt (X11) are loaded at run time, not linked
(cd "$WORK" && mkdir -p shlibs/debian && touch shlibs/debian/control)
DEPENDS=$(cd "$WORK/shlibs" && dpkg-shlibdeps -O -e "$PKG/usr/lib/ritmo/ritmo" | sed 's/^shlibs:Depends=//')
DEPENDS="$DEPENDS, qt6-qpa-plugins"

SIZE=$(du -sk "$PKG/usr" | cut -f1)
cat > "$PKG/DEBIAN/control" <<CONTROL
Package: ritmo
Version: $VERSION
Architecture: $ARCH
Maintainer: Gianluca Renzi <gianlucarenzi@eurek.it>
Installed-Size: $SIZE
Depends: $DEPENDS
Recommends: qt6-wayland
Section: sound
Priority: optional
Homepage: https://github.com/gianlucarenzi/RITMO-Music-Tracker
Description: RITMO, music tracker for the POKEY chip of the Atari XL/XE
 A tracker to compose music for the POKEY sound chip: patterns, instruments,
 the 6502 player, export to the Atari formats (RMT, SAP, XEX...) and
 scripting.
CONTROL

DEB=$WORK/ritmo_${VERSION}_$ARCH.deb
dpkg-deb --root-owner-group --build "$PKG" "$DEB"

# --- checks ------------------------------------------------------------------

dpkg-deb --info "$DEB"
# the dependencies are there (the build installed them already) and the
# installed program starts: the screenshot of gemx.rmt, offscreen
apt-get install -y --no-install-recommends "$DEB"
rm -f "$WORK/ritmo-deb.png"
QT_QPA_PLATFORM=offscreen RMT_QT_GRAB=$WORK/ritmo-deb.png RMT_QT_GRAB_MS=3000 \
    XDG_CONFIG_HOME=$WORK/config-deb timeout 600 ritmo "$ROOT/rmt/songs/rmt128/raster/gemx.rmt" || true
test -f "$WORK/ritmo-deb.png"
ls -la "$DEB"
