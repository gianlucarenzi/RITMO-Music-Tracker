#!/usr/bin/env python3
"""obxheader.py dbgfile PREFIX out.h - the C header MADS writes with -hc, from the debug file of a ca65 build.

MADS -hc: writes one "#define <PREFIX>_<NAME> 0x<VALUE>" for every symbol of the assembled source, in the order the
assembler defines them, with the name in upper case and the prefix taken from the name of the source file. The
tracker addresses the variables of its drivers through these headers (src/tracker_obx.h, src/legacy_obx.h).

Here the same header comes from ca65: obx.sh with OBX_DBG=<file> writes the ld65 debug file, which has every symbol
with its value and the line that defines it. The symbols of mads.inc (OBX_*) and of ca65 itself (CPU_*) are left
out: only those defined in the folder of the source count. The order follows the .include lines of the sources, so
it is the order of assembly, as MADS writes it.
"""
import os
import re
import sys


def fields(line):
    """The fields of a line of the debug file: 'sym<TAB>id=0,name="x",val=0x1' -> {'id': '0', 'name': 'x', ...}"""
    result = {}
    for part in re.findall(r'(\w+)=("[^"]*"|[^,]*)', line.split("\t", 1)[1]):
        result[part[0]] = part[1].strip('"')
    return result


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: obxheader.py dbgfile PREFIX out.h")
    dbgfile, prefix, out = sys.argv[1:]

    files, lines, symbols = {}, {}, []
    for line in open(dbgfile, encoding="utf-8"):
        kind = line.split("\t", 1)[0]
        if kind == "file":
            f = fields(line)
            files[f["id"]] = f["name"]
        elif kind == "line":
            f = fields(line)
            lines[f["id"]] = (f["file"], int(f["line"]))
        elif kind == "sym":
            f = fields(line)
            if "val" in f and "def" in f:
                symbols.append((f["name"], int(f["val"], 16), f["def"].split("+")[0]))

    # the sources: the files of the debug file next to the main source (the first .s of a folder other than the
    # temporary one of obx.sh and mads.inc)
    sources = {fid: path for fid, path in files.items() if path.endswith(".s") and os.path.isabs(path)}
    if not sources:
        sys.exit("obxheader.py: no source files in " + dbgfile)
    folder = os.path.dirname(min(sources.values(), key=len))
    sources = {fid: path for fid, path in sources.items() if os.path.dirname(path) == folder}

    # where each source is included: (parent file id, line), from the .include lines of the sources
    by_name = {os.path.basename(path): fid for fid, path in sources.items()}
    included_at = {}
    for fid, path in sources.items():
        for number, text in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
            m = re.match(r'\s*\.include\s+"([^"]+)"', text)
            if m and os.path.basename(m.group(1)) in by_name:
                included_at[by_name[os.path.basename(m.group(1))]] = (fid, number)

    def order(fid, number):
        """The place of a line in the assembly: the include lines that lead to it, then the line itself"""
        key = (number,)
        while fid in included_at:
            fid, at = included_at[fid]
            key = (at,) + key
        return key

    defines = []
    for name, value, line_id in symbols:
        fid, number = lines[line_id]
        # OBX_*: the symbols of the mads.inc macros (ORG, OPT_H...), which ca65 defines at the line that uses them
        if fid in sources and not name.startswith("OBX_"):
            defines.append((order(fid, number), name.upper(), value))
    defines.sort()

    guard = "_" + prefix + "_ASM_H_"
    with open(out, "w", encoding="utf-8", newline="\n") as h:
        h.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
        for _, name, value in defines:
            h.write("#define %s_%s 0x%04X\n" % (prefix, name, value))
        h.write("\n#endif\n")


main()
