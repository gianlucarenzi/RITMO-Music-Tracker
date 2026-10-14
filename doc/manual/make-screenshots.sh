#!/bin/bash
# Takes the screenshots of the manual from the program itself, offscreen (no display needed), with
# the test hooks of the Qt frontend (see BUILD.md, "Testing without a display"). The images go to
# doc/manual/images/.
#
#   doc/manual/make-screenshots.sh [path/to/ritmo]      (default: build-qt/out/ritmo)
#   ONLY='dialog-*' doc/manual/make-screenshots.sh      only the images whose name matches
#
# The font of the menus and of the dialogs is fixed (RMT_QT_FONT): offscreen Qt takes the one of the
# desktop, or its own small default, and the images would change from a computer to another.
set -eu
cd "$(dirname "$0")/../.."
RMT=$(realpath "${1:-build-qt/out/ritmo}")
[ -x "$RMT" ] || { echo "No such program: $RMT" >&2; exit 2; }
OUT=$PWD/doc/manual/images
mkdir -p "$OUT"
WORK=$(mktemp -d)
# the exports go to a folder with a fixed name: the stripped RMT dialog shows the path of the file
EXPORTS=/tmp/ritmo
rm -rf "$EXPORTS"
mkdir -p "$EXPORTS"
trap 'rm -rf "$WORK" "$EXPORTS"' EXIT
REPO=$PWD
# the defaults, not the settings of the user (the debug display of the status bar is off there)
export XDG_CONFIG_HOME="$WORK/config"
export QT_QPA_PLATFORM=offscreen
export RMT_QT_FONT="DejaVu Sans,12.75"      # 17 pixels, as the images of the manual have always had

SONG=rmt/songs/rmt128/raster/gemx.rmt                 # mono, 4 tracks
STEREO=rmt/songs/rmt128/others/hightide.rmt           # stereo, 8 tracks

# the value of a command ID (resource.h, or the standard ones of CompatTypes.h)
id() {
    grep -h -m1 -E "^#define $1[[:space:]]" src/resource.h src/CompatTypes.h | awk '{print $3}'
}

# shot <name> <song> <ms> [KEY=value ...]: the window after <ms> ms, with the environment given
want() { [ -z "${ONLY:-}" ] || [[ "$1" == $ONLY ]]; }

shot() {
    local name=$1 song=$2 ms=$3
    shift 3
    want "$name" || return 0
    (cd "$WORK" && env "$@" RMT_QT_GRAB="$OUT/$name.png" RMT_QT_GRAB_MS="$ms" "$RMT" ${song:+"$OLDPWD/$song"}) >/dev/null 2>&1 || true
}

# dialog <name> <song> <command> [KEY=value ...]: the dialog the command opens (saved as <name>.png)
dialog() {
    local name=$1 song=$2 cmd=$3
    shift 3
    want "$name" || return 0
    (cd "$WORK" && env "$@" RMT_QT_GRAB="$WORK/window.png" RMT_QT_GRAB_MS=3000 RMT_QT_COMMANDS="$cmd" \
        RMT_QT_DIALOG="$OUT/$name.png" "$RMT" ${song:+"$OLDPWD/$song"}) >/dev/null 2>&1 || true
}

# the main window in its modes
shot window-tracks "$SONG" 1500
shot window-instruments "$SONG" 2000 RMT_QT_COMMANDS="$(id ID_INSTR_LOAD),$(id ID_EM_INSTRUMENTS)" RMT_QT_FILEDIALOG="$REPO/rmt/instruments/pure9_5420.rti"
shot window-song "$SONG" 1500 RMT_QT_COMMANDS="$(id ID_EM_SONG)"
shot window-info "$SONG" 1500 RMT_QT_COMMANDS="$(id ID_EM_INFO)"
shot window-stereo "$STEREO" 1500
shot window-playing "$STEREO" 6000 RMT_AUDIO_DUMP="$WORK/play.wav" RMT_QT_COMMANDS="$(id ID_PLAY1)"
# (the volume analyzer and the POKEY registers are on by default: their commands would
# switch them off; the registers have room on the screen with the 4 tracks of a mono song)
shot window-explorer "$SONG" 2500 RMT_QT_COMMANDS="$(id ID_EDIT_ACTIVATE_POKEY_EXPLORER_MODE)"
shot window-analyzer "$SONG" 5000 RMT_AUDIO_DUMP="$WORK/play2.wav" RMT_QT_COMMANDS="$(id ID_PLAY1)"

# the menus
shot window-menus "$SONG" 1500 RMT_QT_MENU_GRAB="$OUT/menu"

# the dialogs
dialog dialog-new "" "$(id ID_FILE_NEW)"
dialog dialog-options "$SONG" "$(id ID_VIEW_CONFIGURATION)"
dialog dialog-tuning "$SONG" "$(id ID_VIEW_TUNING)"
dialog dialog-block-effects "$SONG" "$(id ID_BLOCK_SELECTALL),$(id ID_BLOCK_EFFECT)"
dialog dialog-columns "$SONG" "$(id ID_SONG_TRACKSORDERCHANGE)"
dialog dialog-max-length "$SONG" "$(id ID_SONG_SONGCHANGEMAXIMALLENGTHOFTRACKS)"
dialog dialog-4-8 "$SONG" "$(id ID_SONG_SONGSWITCH4_8)"
dialog dialog-copy-lines "$SONG" "$(id ID_SONG_INSERTCOPYORCLONEOFSONGLINES)"
dialog dialog-instrument-info "$SONG" "$(id ID_INSTRUMENT_INFO)"
dialog dialog-track-info "$SONG" "$(id ID_TRACK_INFOABOUTUSINGOFACTUALTRACK)"
dialog dialog-size-optimization "$SONG" "$(id ID_SONG_SIZEOPTIMIZATION)"
dialog dialog-import "" "$(id ID_FILE_IMPORT)" RMT_QT_FILEDIALOG="$REPO/rmt/imports/axel_f.mod"
# the export dialogs: the file dialog is answered with a name and the file type (file@N)
n=0
for fmt in "stripped-rmt:1" "simple-asm:2" "sapr:3" "sap:5" "xex:6" "reloc-asm:7"; do
    name=${fmt%%:*}
    idx=${fmt##*:}
    ext=rmt
    [ "$name" = sapr ] && ext=sapr
    [ "$name" = sap ] && ext=sap
    [ "$name" = xex ] && ext=xex
    case $name in simple-asm | reloc-asm) ext=asm ;; esac
    dialog "dialog-export-$name" "$SONG" "$(id ID_FILE_EXPORT_AS)" RMT_QT_FILEDIALOG="$EXPORTS/gemx-$name.$ext@$idx"
    n=$((n + 1))
done

# the dialogs that open after another one (the second image of the dialog hook)
for f in "$OUT"/dialog-*-2.png; do [ -e "$f" ] && echo "second image: $f"; done || true
ls "$OUT" | wc -l
