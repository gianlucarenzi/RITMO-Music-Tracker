#!/usr/bin/env python3
"""Checks the structure of an XEX written by "export xex-rmt" (the XEX with the RMT player, asm/rmtplay): which
player it holds (RMT 1.28 at $3400 or Patch16 at $3100 with its tables at $3A00, mono or stereo), that the module
(RMT4 / RMT8 as expected) is where src/rmt_xex.h says and rmt_song / rmt_song_end point to it, that the run address
is in a block and that the NAME line is the one given.

    scripts/check-xex-rmt.py file.xex tracks name
"""
import os
import re
import sys


def fail(message):
    print("FAIL %s: %s" % (sys.argv[1], message))
    sys.exit(1)


header = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "rmt_xex.h")
defines = dict(re.findall(r"#define (RMTXEX_\w+) (0x[0-9A-Fa-f]+|\d+)", open(header).read()))
data = open(sys.argv[1], "rb").read()
tracks = int(sys.argv[2])
name = sys.argv[3]
if data[:2] != b"\xff\xff":
    fail("no Atari binary header")

memory = bytearray(65536)
loaded = [False] * 65536
p = 0
while p + 4 <= len(data):
    if data[p:p + 2] == b"\xff\xff":
        p += 2
        continue
    first = data[p] | data[p + 1] << 8
    last = data[p + 2] | data[p + 3] << 8
    p += 4
    if last < first or p + last - first + 1 > len(data):
        fail("a binary block is cut off")
    memory[first:last + 1] = data[p:p + last - first + 1]
    for a in range(first, last + 1):
        loaded[a] = True
    p += last - first + 1

if loaded[0x3A00] and loaded[0x3100]:
    driver = "V6"
elif loaded[0x3400]:
    driver = "V1"
else:
    fail("neither the RMT 1.28 player ($3400) nor the Patch16 player ($3100, tables at $3A00)")
variant = "%s_%s" % (driver, "STEREO" if tracks == 8 else "MONO")


def symbol(s):
    return int(defines["RMTXEX_%s_%s" % (variant, s)], 0)


module = symbol("MODULE")
if memory[module:module + 4] != b"RMT%d" % tracks:
    fail("no RMT%d module at %04X" % (tracks, module))
song = symbol("RMT_SONG")
start = memory[song] | memory[song + 1] << 8
end = memory[song + 2] | memory[song + 3] << 8
if start != module or not loaded[end - 1] or end > int(defines["RMTXEX_MODULE_LIMIT"], 0):
    fail("rmt_song / rmt_song_end (%04X / %04X) do not point to the module" % (start, end))
if not (loaded[0x2E0] and loaded[0x2E1]):
    fail("no run address")
run = memory[0x2E0] | memory[0x2E1] << 8
if not loaded[run]:
    fail("the run address %04X is not in a block" % run)
info = symbol("RMT_INFO")
line = bytes(memory[info:info + int(defines["RMTXEX_INFO_MAX"]) + 1]).split(b"\0")[0].decode("latin-1")
if line != name:
    fail("the NAME line is %r, not %r" % (line, name))
print("ok   %s: %s player, RMT%d module at $%04X-$%04X, run %04X, NAME %r"
      % (sys.argv[1].rsplit("/", 1)[-1], "Patch16" if driver == "V6" else "RMT 1.28", tracks, start, end - 1, run, line))
