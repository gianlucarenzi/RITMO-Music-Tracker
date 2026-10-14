#!/usr/bin/env python3
"""Checks the structure of an XEX written by "export xex-rmt" (the XEX with the RMT player): the binary blocks, that a
block at $4000 holds the RMT module (RMT4 / RMT8 as expected), that the run address is in a block, which player it is
(RMT 1.28: the XEX routine at $3E00 and its text at $3F00; Patch16: the Simple RMT Player, its text at $A190) and that
the text of the first line is there.

    scripts/check-xex-rmt.py file.xex tracks first-line
"""
import sys


def fail(message):
    print("FAIL %s: %s" % (sys.argv[1], message))
    sys.exit(1)


data = open(sys.argv[1], "rb").read()
tracks = int(sys.argv[2])
first_line = sys.argv[3]
if data[:2] != b"\xff\xff":
    fail("no Atari binary header")

memory = bytearray(65536)
loaded = [False] * 65536
p = 0
while p + 4 <= len(data):
    if data[p:p + 2] == b"\xff\xff":
        p += 2
    first = data[p] | data[p + 1] << 8
    last = data[p + 2] | data[p + 3] << 8
    p += 4
    if last < first or p + last - first + 1 > len(data):
        fail("a binary block is cut off")
    memory[first:last + 1] = data[p:p + last - first + 1]
    for a in range(first, last + 1):
        loaded[a] = True
    p += last - first + 1

if memory[0x4000:0x4004] != b"RMT%d" % tracks:
    fail("no RMT%d module at $4000" % tracks)
if not (loaded[0x2E0] and loaded[0x2E1]):
    fail("no run address")
run = memory[0x2E0] | memory[0x2E1] << 8
if not loaded[run]:
    fail("the run address %04X is not in a block" % run)
if loaded[0xA190] and loaded[0xB000] and loaded[0xBC00]:
    player, text = "Simple RMT Player", 0xA190
elif loaded[0x3E00] and loaded[0x3F00]:
    player, text = "RMT 1.28", 0x3F00
else:
    fail("neither the RMT 1.28 player nor the Simple RMT Player")
# the text as ANTIC screen codes (SongExporter::StrToAtariVideo)
codes = bytes((c - 32) if 32 <= c < 96 else c for c in first_line.encode("latin-1"))
if memory[text:text + len(codes)] != codes:
    fail("the first text line at %04X is not %r" % (text, first_line))
print("ok   %s: %s, RMT%d module at $4000, run %04X, text at %04X" % (sys.argv[1].rsplit("/", 1)[-1], player, tracks, run, text))
