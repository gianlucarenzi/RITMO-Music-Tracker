#!/usr/bin/env python3
"""Checks the structure of a SAP file of type B written by "export sap": the header, that the binary blocks
cover the INIT and PLAYER addresses and every address the code at INIT jumps to (the earlier export left the
player's driver out, so no player could run it), that the loop counter stays at $FF (endless loop: an earlier
export let INIT clear it, and the player faded out at the second end of the loop), and that the song index has one
entry per subsong.

    scripts/check-sap.py file.sap [songs]
"""
import sys


def fail(message):
    print("FAIL %s: %s" % (sys.argv[1], message))
    sys.exit(1)


data = open(sys.argv[1], "rb").read()
end = data.find(b"\r\n\r\n")
if not data.startswith(b"SAP\r\n") or end < 0:
    fail("no SAP header")
tags = {}
for line in data[5:end].decode("latin-1").split("\r\n"):
    key, _, value = line.partition(" ")
    tags[key] = value
if tags.get("TYPE") != "B":
    fail("TYPE is %r, not B" % tags.get("TYPE"))
init = int(tags["INIT"], 16)
player = int(tags["PLAYER"], 16)
songs = int(tags.get("SONGS", "1"))
if len(sys.argv) > 2 and songs != int(sys.argv[2]):
    fail("SONGS %d, expected %s" % (songs, sys.argv[2]))

memory = bytearray(65536)
loaded = [False] * 65536
p = end + 4
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

for name, address in (("INIT", init), ("PLAYER", player)):
    if not loaded[address]:
        fail("%s %04X is not in a block" % (name, address))
# INIT is a stub ending in JMP: the target must be loaded too
for a in range(init, init + 14):
    if memory[a] == 0x4C:  # JMP
        target = memory[a + 1] | memory[a + 2] << 8
        if not loaded[target]:
            fail("INIT jumps to %04X which is not in a block" % target)
        break
else:
    fail("no JMP in the INIT stub")
if memory[0x1990:0x1993] != bytes([0x60, 0xEA, 0xEA]):
    fail("the player's main loop is not patched to RTS")
# endless loop: the loop counter at $0D3E is $FF and INIT (SetNewSongPtrsFull) does not clear it (STX $0D3E -> NOPs),
# otherwise the player fades out at the second end of the loop
if memory[0x0D3E] != 0xFF or memory[0x0CE3:0x0CE6] != bytes([0xEA, 0xEA, 0xEA]):
    fail("the loop counter is not kept at $FF (endless loop)")
# the song index at $1F40: 4 bytes per subsong (section list address, sequence list address), all inside the data
for song in range(songs):
    e = 0x1F40 + 4 * song
    section = memory[e] | memory[e + 1] << 8
    sequence = memory[e + 2] | memory[e + 3] << 8
    if not (loaded[section] and loaded[sequence]):
        fail("subsong %d: its lists (%04X, %04X) are not in a block" % (song, section, sequence))
print("ok   %s: %d subsong(s), INIT %04X, PLAYER %04X, blocks cover them" % (sys.argv[1].rsplit("/", 1)[-1], songs, init, player))
