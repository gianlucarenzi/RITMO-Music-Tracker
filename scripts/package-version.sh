#!/usr/bin/env bash
# scripts/package-version.sh - the version in the names of the packages
#
# Prints the version that the AppImage, the .tar.gz, the DMG, the ZIP and the
# installer carry in their names (Ritmo-2.6-rc3-x86_64.AppImage):
# - on a tag v*: the tag without the v (v2.6-rc3 -> 2.6-rc3), the version the
#   program itself shows;
# - after a tag: git describe (2.6-rc3-4-g43f47b8), so that a test build is
#   not taken for a release;
# - without git or tags: RMT_BASE_VERSION of CMakeLists.txt.
# Works with the bash of Linux, macOS and MSYS2.

cd "$(dirname "$0")/.."
if DESCRIBE=$(git describe --tags --match 'v[0-9]*' 2>/dev/null); then
    echo "${DESCRIBE#v}"
else
    sed -nE 's/^[[:space:]]*set\(RMT_BASE_VERSION[[:space:]]+"([^"]+)".*/\1/p' CMakeLists.txt | head -1
fi
