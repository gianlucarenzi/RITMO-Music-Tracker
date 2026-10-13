#!/bin/bash

# Downloads the AppImage of a release of RITMO
# Usage: ./download-appimage.sh <version> [directory]
# E.g.:  ./download-appimage.sh 2.4
# E.g.:  ./download-appimage.sh 2.4 ~/Downloads
#
# Needs the GitHub CLI (gh). The AppImage of the architecture of this machine
# (x86_64 or aarch64) is saved as Ritmo-<version>-<arch>.AppImage
# (RMT-<version>-x86_64.AppImage for the releases up to 2.2.1, which were called RMT).

set -e

VERSION="${1:?Error: give the version (e.g. 2.4)}"
OUTPUT_DIR="${2:-.}"
REPO="gianlucarenzi/RITMO-Music-Tracker"

# The architecture: since 2.5 a release has an AppImage for x86_64 and one for aarch64
ARCH=$(uname -m)
case "$ARCH" in
    x86_64|aarch64) ;;
    arm64) ARCH=aarch64 ;;
    *) echo "❌ Error: no AppImage for $ARCH (x86_64 and aarch64 only)"; exit 1 ;;
esac

# The tag of the release
TAG="v${VERSION}"

echo "📥 Downloading the $ARCH AppImage of version $VERSION from $REPO..."
echo "   Tag: $TAG"
echo "   Destination: $OUTPUT_DIR"

# Download the AppImage into a temporary folder
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

if ! gh release download "$TAG" --repo "$REPO" --pattern "*-Linux-${ARCH}*.AppImage" --pattern "Ritmo-*-${ARCH}.AppImage" --dir "$TEMP_DIR"; then
    echo "❌ Error: cannot download version $VERSION"
    echo "   Check that the tag $TAG exists in the repository"
    exit 1
fi

# Find the AppImage that was downloaded
APPIMAGE_FILE=$(ls "$TEMP_DIR"/*.AppImage 2>/dev/null | head -1)
if [ -z "$APPIMAGE_FILE" ]; then
    echo "❌ Error: no AppImage file found in release $VERSION"
    exit 1
fi

# The name of the output file: Ritmo-... from 2.3, RMT-... up to 2.2.1 (up to 2.5
# the AppImage of the release was Ritmo-Linux-<arch>.AppImage, from 2.6 it is
# Ritmo-<version>-<arch>.AppImage: the same name)
PREFIX=$(basename "$APPIMAGE_FILE" | sed 's/-.*//')
OUTPUT_FILE="$OUTPUT_DIR/${PREFIX}-${VERSION}-${ARCH}.AppImage"

# Move the file and make it executable
mv "$APPIMAGE_FILE" "$OUTPUT_FILE"
chmod +x "$OUTPUT_FILE"

echo "✅ Download complete!"
echo "   File: $OUTPUT_FILE"
echo "   Size: $(du -h "$OUTPUT_FILE" | cut -f1)"
echo ""
echo "   Run it with: $OUTPUT_FILE"
