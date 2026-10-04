#!/usr/bin/env python3
"""Converts a Markdown document of doc/ to a LaTeX fragment with pandoc.

The reference documents of the program (the keys, the instrument fields, the scripting commands,
the tracker drivers) stay in Markdown, where the generated tables come from the program itself
(scripts/make-docs.sh); the manual includes them through this script, so that it never drifts.

    md2tex.py <file.md> <out.tex> [--shift N] [--from-heading TEXT] [--to-heading TEXT]

--shift N          the first level headings of the file become level N (1 = chapter in the book class,
                   2 = section, ...)
--from-heading     start at this heading (without it)
--to-heading       stop before this heading
--drop-rows REGEX  leave out the table rows that match (the 100 rows of the per channel Pokey menu)
"""
import argparse
import os
import re
import subprocess
import sys

ap = argparse.ArgumentParser()
ap.add_argument("src")
ap.add_argument("out")
ap.add_argument("--shift", type=int, default=1)
ap.add_argument("--from-heading")
ap.add_argument("--to-heading")
ap.add_argument("--drop-rows", help="regular expression: the table rows that match are left out")
args = ap.parse_args()

here = os.path.dirname(os.path.abspath(args.src))
text = open(args.src, encoding="utf-8-sig").read()


def include(m):
    path = os.path.join(here, m.group(1))
    return open(path, encoding="utf-8-sig").read()


text = re.sub(r"<!-- include: (\S+) -->", include, text)
text = re.sub(r"<a id=['\"][^'\"]*['\"]></a>", "", text)
text = text.replace("<br>", " / ")

# Setext headings ("Title" over "-----") to ATX, the glossary of rmt_tracker.md
text = re.sub(r"^(?!#)(\S[^\n]*)\n(=+|-{3,})\n", lambda m: ("# " if m.group(2)[0] == "=" else "## ") + m.group(1) + "\n", text, flags=re.M)

# the part wanted
if args.from_heading or args.to_heading:
    lines = text.split("\n")
    start, end = 0, len(lines)
    for i, l in enumerate(lines):
        if args.from_heading and re.match(r"#+\s+" + re.escape(args.from_heading) + r"\s*$", l):
            start = i + 1
        if args.to_heading and i > start and re.match(r"#+\s+" + re.escape(args.to_heading) + r"\s*$", l):
            end = i
            break
    text = "\n".join(lines[start:end])


def table_widths(block):
    """The relative widths of a pipe table: the dashes of the separator line follow the longest cell."""
    rows = [[c.strip() for c in l.strip().strip("|").split("|")] for l in block if not re.match(r"^\s*>?\s*\|[\s:|-]+\|\s*$", l)]
    sep = next(i for i, l in enumerate(block) if re.match(r"^\s*>?\s*\|[\s:|-]+\|\s*$", l))
    n = len(rows[0])
    def width(cell):  # a mono type cell (backquotes) is wider
        return int(len(cell) * (1.6 if "`" in cell else 1))
    longest = [min(max((width(r[c]) if c < len(r) else 0) for r in rows), 70) for c in range(n)]
    longest = [max(10, x) + 4 for x in longest]
    seps = "|" + "|".join("-" * x for x in longest) + "|"
    return sep, seps


if args.drop_rows:
    drop = re.compile(args.drop_rows)
    text = "\n".join(l for l in text.split("\n") if not (l.lstrip().startswith("|") and drop.search(l)))

out = []
lines = text.split("\n")
i = 0
while i < len(lines):
    if re.match(r"^\s*>?\s*\|", lines[i]):
        j = i
        while j < len(lines) and re.match(r"^\s*>?\s*\|", lines[j]):
            j += 1
        block = lines[i:j]
        quoted = block[0].lstrip().startswith(">")
        block = [re.sub(r"^\s*>\s*", "", l) for l in block]
        try:
            sep, seps = table_widths(block)
            block[sep] = seps
        except StopIteration:
            pass
        if quoted:
            block = [""] + block + [""]
        out += block
        i = j
    else:
        out.append(lines[i])
        i += 1
text = "\n".join(out)

cmd = [
    "pandoc",
    "-f", "markdown-smart+gfm_auto_identifiers+pipe_tables-raw_html",
    "-t", "latex",
    "--columns=60",
    f"--shift-heading-level-by={args.shift - 1}",
    "--top-level-division=chapter",
]
res = subprocess.run(cmd, input=text, capture_output=True, text=True)
if res.returncode:
    sys.exit(res.stderr)
tex = res.stdout
# the code blocks as listings, which break the long lines
tex = tex.replace("\\begin{verbatim}", "\\begin{lstlisting}").replace("\\end{verbatim}", "\\end{lstlisting}")

# the tables without widths (tab separated "simple" tables of rmt_format.md): the last column wraps
def wrap_last(m):
    n = len(m.group(1))
    if n < 2 or n > 3:
        return m.group(0)
    last = "\\raggedright\\arraybackslash}p{" + ("0.62" if n == 2 else "0.62") + "\\columnwidth}"
    return "\\begin{longtable}[]{@{}" + "l" * (n - 1) + ">{" + last + "@{}}"
tex = re.sub(r"\\begin\{longtable\}\[\]\{@\{\}(l+)@\{\}\}", wrap_last, tex)

# a long word in mono type (names with _ or -) may be broken at those characters
def breakable(m):
    inner = m.group(1)
    inner = inner.replace("\\_", "\\_\\allowbreak{}").replace("-", "-\\allowbreak{}")
    return "\\texttt{" + inner + "}"
tex = re.sub(r"\\texttt\{([^{}]*)\}", breakable, tex)
tex = re.sub(r"\\_(?!\\allowbreak)", r"\\_\\allowbreak{}", tex)

# the tables with many columns in a smaller type and with less space between the columns
def small(m):
    body = m.group(0)
    cols = re.match(r"\\begin\{longtable\}\[\]\{@\{\}(\w+)@\{\}\}", body)
    if cols and len(cols.group(1)) >= 5:
        return "{\\footnotesize\\setlength{\\tabcolsep}{3pt}\n" + body + "\n}"
    return body
tex = re.sub(r"\\begin\{longtable\}.*?\\end\{longtable\}", small, tex, flags=re.S)
# the headings get the identifiers of GitHub (the anchors the links of the Markdown file use), with the
# name of the output file in front, as two documents may have a heading with the same name; a link to a
# heading left out of the book (--from-heading, --to-heading) keeps the text only
prefix = os.path.splitext(os.path.basename(args.out))[0] + ":"
tex = re.sub(r"\\(hypertarget|label)\{([^}]*)\}", lambda m: "\\" + m.group(1) + "{" + prefix + m.group(2) + "}", tex)
labels = set(re.findall(r"\\label\{([^}]*)\}", tex))
def internal_link(m):
    target = prefix + m.group(1)
    return "\\hyperref[" + target + "]{" + m.group(2) + "}" if target in labels else m.group(2)
tex = re.sub(r"\\hyperref\[([^\]]*)\]\{((?:[^{}]|\{[^{}]*\})*)\}", internal_link, tex)
# the links to the other Markdown files of doc/ have no target in the book: the text only
tex = re.sub(r"\\href\{[^}]*\.md[^}]*\}\{((?:[^{}]|\{[^{}]*\})*)\}", r"\1", tex)
open(args.out, "w", encoding="utf-8").write(tex)
