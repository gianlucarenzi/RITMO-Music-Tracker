#!/bin/bash
# Regenerates the documents that come from the program itself, with the script commands
# "dump actions" and "dump notekeys":
#   doc/rmt_action_infos.md   every menu item and toolbar button with its key and description
#   doc/rmt_note_keys.md      the note keys of the QWERTY, QWERTZ and AZERTY layouts
# and, with pandoc, doc/rmt_en.html: the reference of RMT with those tables in it. The user manual
# (doc/manual/ritmo-manual.pdf) is built by doc/manual/build.sh.
#
#   scripts/make-docs.sh [path/to/ritmo]      (default: build-qt/out/ritmo)
set -eu
cd "$(dirname "$0")/.."
RMT=${1:-build-qt/out/ritmo}
[ -x "$RMT" ] || { echo "No such program: $RMT" >&2; exit 2; }
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
export XDG_CONFIG_HOME="$WORK/config"      # the defaults, not the settings of the user
cat >"$WORK/docs.rmtscript" <<SCRIPT
set overwrite yes
dump actions $PWD/doc/rmt_action_infos.md
dump notekeys $PWD/doc/rmt_note_keys.md
SCRIPT
"$RMT" "/SCRIPT:$WORK/docs.rmtscript"

# The reference of RMT: doc/rmt_en.md with the generated tables included, as HTML (doc/rmt_en.html), when pandoc is there
if command -v pandoc >/dev/null; then
    awk -v dir=doc '
        /^<!-- include: .* -->$/ { f = $3; while ((getline line < (dir "/" f)) > 0) print line; close(dir "/" f); next }
        { print }' doc/rmt_en.md >"$WORK/rmt_en.md"
    cat >"$WORK/style.html" <<'STYLE'
<style>
body { font-family: sans-serif; max-width: 62em; margin: 1em auto; padding: 0 1em; }
table { border-collapse: collapse; margin: 1em 0; }
th, td { border: 1px solid #c0c0c0; padding: 0.2em 0.5em; vertical-align: top; }
th { background: #e0e0e0; }
code { background: #f0f0f0; }
pre { background: #f0f0f0; padding: 0.5em; overflow-x: auto; }
img { max-width: 100%; }
</style>
STYLE
    pandoc -f gfm -t html5 -s --metadata title="RITMO - Manual" -H "$WORK/style.html" "$WORK/rmt_en.md" -o doc/rmt_en.html
    echo "Wrote doc/rmt_en.html"
else
    echo "pandoc not found: doc/rmt_en.html not regenerated"
fi
