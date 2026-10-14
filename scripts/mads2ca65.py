#!/usr/bin/env python3
"""mads2ca65 - translate the MADS/XASM sources of asm/ into ca65 sources.

    scripts/mads2ca65.py [asm-folder [output-folder]]   (default: asm asm/ca65)

Every *.a65 / *.asm file of the asm folder (Legacy, Legacy/rmf, Patch-16) is
written as <name>.s in the same place under the output folder. The output is
meant for ca65 -i (identifiers not case sensitive, as in MADS) with
asm/ca65/mads.inc, which provides ORG, OPT_H, RUN and INI: the segments of the
Atari binary file ($FFFF, start, end) are made as MADS makes them. See
asm/ca65/README.md.

Translated:
  label / label equ x / label org *+n    label = * / label = x / label = * + ORG *+n
  org / opt h+ h- / run / ini            ORG / OPT_H 1 0 / RUN / INI (mads.inc)
  icl / ins                              .include / .incbin
  IFT ELI ELS EIF / ERT                  .if .elseif .else .endif / .assert
  dta b() a() l() h() c"" d"" "..."*     .byte / .word (strings as bytes)
  [ ] == != @                            ( ) = <> a
  :n instruction (# = the counter)       .repeat n, I ... .endrep
  mva mwa mwx add sub jeq.. scc:.. ..:rne the 6502 instructions MADS makes of them
  lda addr,x+ / lda <addr                lda addr,x + inx / lda #<addr
"""

import os
import re
import sys

OPCODES = set("""adc and asl bcc bcs beq bit bmi bne bpl brk bvc bvs clc cld cli clv cmp cpx cpy dec dex dey eor
inc inx iny jmp jsr lda ldx ldy lsr nop ora pha php pla plp rol ror rti rts sbc sec sed sei sta stx sty tax tay
tsx txa txs tya""".split())

# condition of the skip (sXX:) and repeat (:rXX) and long branch (jXX) pseudo instructions -> branch
CONDS = {"eq": "beq", "ne": "bne", "cc": "bcc", "cs": "bcs", "pl": "bpl", "mi": "bmi", "vc": "bvc", "vs": "bvs"}
NEGATE = {"beq": "bne", "bne": "beq", "bcc": "bcs", "bcs": "bcc", "bpl": "bmi", "bmi": "bpl", "bvc": "bvs", "bvs": "bvc"}

# configuration symbols: a definition becomes ".ifndef X / X = ... / .endif" so that ca65 -D X=... chooses them
CONFIG_SYMBOLS = {"STEREOMODE", "EXPORTXEX", "EXPORTSAP", "REGIONPLAYBACK", "STARTLINE", "VLINE", "DISPLAYNOTES",
                  "RITMOEXPORT"}

REPEAT_COUNTER = "I"


def split_comment(line):
    """code, comment (with its ';') - a ';' inside quotes is not a comment"""
    quote = None
    for i, ch in enumerate(line):
        if quote:
            if ch == quote:
                quote = None
        elif ch in "\"'":
            # a quote right after a letter or digit is not a string (no such case in the sources, kept simple)
            quote = ch
        elif ch == ";":
            return line[:i], line[i:]
    return line, ""


def first_token(s):
    """the first blank-separated token of s, blanks inside quotes or brackets do not separate"""
    quote = None
    depth = 0
    for i, ch in enumerate(s):
        if quote:
            if ch == quote:
                quote = None
        elif ch in "\"'":
            quote = ch
        elif ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif ch in " \t" and depth <= 0:
            return s[:i], s[i:].strip()
    return s, ""


def split_list(s):
    """split at the commas outside quotes and brackets"""
    items, cur, quote, depth = [], "", None, 0
    for ch in s:
        if quote:
            cur += ch
            if ch == quote:
                quote = None
            continue
        if ch in "\"'":
            quote = ch
        elif ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif ch == "," and depth == 0:
            items.append(cur.strip())
            cur = ""
            continue
        cur += ch
    if cur.strip():
        items.append(cur.strip())
    return items


def expr(e):
    """a MADS expression as a ca65 expression"""
    out, i = "", 0
    while i < len(e):
        ch = e[i]
        # a one character string is its ATASCII code
        if ch in "\"'" and i + 2 < len(e) and e[i + 2] == ch:
            out += "$%02X" % ord(e[i + 1])
            i += 3
            continue
        if e.startswith("==", i):
            out += "="
            i += 2
            continue
        if e.startswith("!=", i):
            out += "<>"
            i += 2
            continue
        out += {"[": "(", "]": ")"}.get(ch, ch)
        i += 1
    return out


def operand(op):
    """an instruction operand: accumulator, brackets"""
    if op == "@":
        return "a"
    e = expr(op)
    if e.startswith("#-"):
        return "#<(%s)" % e[1:]  # MADS takes a negative byte as its two's complement
    if op.startswith("["):
        e = "0+" + e  # a leading bracket is grouping in MADS, ca65 would read "(" as indirect
    return e


def atascii_to_internal(c):
    inv = c & 0x80
    c &= 0x7F
    if c < 0x20:
        c += 0x40
    elif c < 0x60:
        c -= 0x20
    return c | inv


def string_bytes(kind, text, inverse):
    data = [ord(c) & 0xFF for c in text]
    if kind == "d":
        data = [atascii_to_internal(c) for c in data]
    if inverse:
        data = [c | 0x80 for c in data]
    return data


STRING_RE = re.compile(r"""^([cdCD]?)(["'])(.*)\2(\*?)$""", re.S)
FUNC_RE = re.compile(r"^([bBaAlLhH])\((.*)\)$", re.S)


def dta(args):
    """lines of .byte/.word for the dta arguments"""
    lines, pending = [], []

    def flush():
        if pending:
            lines.append("\t.byte\t" + ", ".join(pending))
            pending.clear()

    for item in split_list(args):
        m = STRING_RE.match(item)
        if m and (m.group(1) or len(m.group(3)) != 1):  # "x" alone is a character code
            kind = m.group(1).lower()
            text = m.group(3)
            data = string_bytes(kind, text, m.group(4) == "*")
            if kind != "d" and not m.group(4) and all(32 <= ord(c) < 127 and c != '"' for c in text):
                pending.append('"%s"' % text)
            else:
                flush()
                lines.append("\t.byte\t" + ", ".join("$%02X" % b for b in data) + "\t; " + item)
            continue
        m = FUNC_RE.match(item)
        if m:
            kind = m.group(1).lower()
            values = [expr(v) for v in split_list(m.group(2))]
            if kind == "a":
                flush()
                lines.append("\t.word\t" + ", ".join(values))
            elif kind == "b":
                pending.extend("<(%s)" % v if v.startswith("-") else v for v in values)
            elif kind == "l":
                pending.extend("<(%s)" % v for v in values)
            else:
                pending.extend(">(%s)" % v for v in values)
            continue
        v = expr(item)
        pending.append("<(%s)" % v if v.startswith("-") else v)
    flush()
    return lines


def split_index(op):
    """operand with a MADS ,x+ ,y- ... suffix -> operand, register step instruction or None"""
    m = re.match(r"^(.*,\s*([xyXY]))([+-])$", op)
    if not m:
        return op, None
    reg = m.group(2).lower()
    return m.group(1), ("in" if m.group(3) == "+" else "de") + reg


def pseudo(mnemonic, args):
    """the 6502 lines of a MADS pseudo instruction, or None if it is not one"""
    parts = mnemonic.lower().split(":")
    # sXX:instr - skip the instruction when the condition is true
    if len(parts) == 2 and parts[0][0] == "s" and parts[0][1:] in CONDS:
        inner = instruction(parts[1], args)
        return ["\t%s\t:+" % CONDS[parts[0][1:]]] + inner + [":"]
    # instr:rXX - repeat the instruction while the condition is true
    if len(parts) == 2 and parts[1][0] == "r" and parts[1][1:] in CONDS:
        inner = instruction(parts[0], args)
        return [":"] + inner + ["\t%s\t:-" % CONDS[parts[1][1:]]]
    m = mnemonic.lower()
    ops = args.split()
    if m in ("mva", "mvx", "mvy"):
        reg = {"mva": "a", "mvx": "x", "mvy": "y"}[m]
        src, s1 = split_index(ops[0])
        dst, s2 = split_index(ops[1])
        lines = ["\tld%s\t%s" % (reg, operand(src)), "\tst%s\t%s" % (reg, operand(dst))]
        for step in (s1, s2):
            if step and "\t" + step not in lines:
                lines.append("\t" + step)
        return lines
    if m in ("mwa", "mwx", "mwy"):
        reg = {"mwa": "a", "mwx": "x", "mwy": "y"}[m]
        src, dst = ops[0], ops[1]
        if src.startswith("#"):
            v = expr(src[1:])
            lo, hi = "#<(%s)" % v, "#>(%s)" % v
        else:
            lo, hi = operand(src), operand(src) + "+1"
        return ["\tld%s\t%s" % (reg, lo), "\tst%s\t%s" % (reg, operand(dst)),
                "\tld%s\t%s" % (reg, hi), "\tst%s\t%s+1" % (reg, operand(dst))]
    if m == "add":
        return ["\tclc", "\tadc\t" + operand(ops[0])]
    if m == "sub":
        return ["\tsec", "\tsbc\t" + operand(ops[0])]
    if m[0] == "j" and m[1:] in CONDS:
        return ["\t%s\t:+" % NEGATE[CONDS[m[1:]]], "\tjmp\t" + operand(ops[0]), ":"]
    return None


def pseudo_instruction(mnemonic):
    m = mnemonic.lower()
    return ":" in m or m in ("mva", "mvx", "mvy", "mwa", "mwx", "mwy", "add", "sub") or (m[0] == "j" and m[1:] in CONDS)


def instruction(mnemonic, args):
    lines = pseudo(mnemonic, args)
    if lines is not None:
        return lines
    m = mnemonic.lower()
    if m not in OPCODES:
        raise ValueError("unknown instruction " + mnemonic)
    op, _ = first_token(args) if args else ("", "")
    if not op:
        return ["\t" + m]
    op, step = split_index(op)  # MADS: sta addr,x+ is sta addr,x / inx
    if op[0] in "<>":
        op = "#" + op  # MADS: lda <addr is lda #<addr
    lines = ["\t%s\t%s" % (m, operand(op))]
    if step:
        lines.append("\t" + step)
    return lines


class Converter:
    def __init__(self, src_root, out_root):
        self.src_root = src_root
        self.out_root = out_root

    def out_name(self, name):
        return re.sub(r"\.(a65|asm)$", ".s", name, flags=re.I)

    def convert_file(self, path):
        rel = os.path.relpath(path, self.src_root)
        config_file = "feat" in os.path.basename(path).lower()
        out = ["; %s - ca65 version of asm/%s" % (os.path.basename(self.out_name(path)), rel),
               "; Generated by scripts/mads2ca65.py: edit the MADS source and run the script again.",
               ""]
        with open(path, encoding="latin-1") as f:
            for n, raw in enumerate(f, 1):
                raw = raw.rstrip("\r\n")
                try:
                    out.extend(self.convert_line(raw, config_file))
                except Exception as e:
                    raise SystemExit("%s:%d: %s\n  %s" % (path, n, e, raw))
        dst = os.path.join(self.out_root, self.out_name(rel))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "w", encoding="latin-1") as f:
            f.write("\n".join(out) + "\n")
        return dst

    def convert_line(self, raw, config_file):
        if raw.startswith("*"):  # XASM/MADS full line comment
            return [";" + raw]
        code, comment = split_comment(raw)
        if not code.strip():
            return [raw.rstrip()]
        label = None
        if code[0] not in " \t":
            label, rest = first_token(code)
        else:
            rest = code.strip()
        if not rest:
            return ["%s = *%s" % (label, "\t" + comment if comment else "")]
        mnemonic, args = first_token(rest)
        low = mnemonic.lower()

        # :n repeat
        repeat = None
        if re.match(r"^:\d+$", mnemonic):
            repeat = int(mnemonic[1:])
            mnemonic, args = first_token(args)
            low = mnemonic.lower()
            # "#" is the counter, except at the start of an operand where it means immediate
            args = re.sub(r"(?<=[^\s,])#", REPEAT_COUNTER, args)

        def labelled(body):
            return (["%s = *" % label] if label else []) + body

        if low == "equ":
            value = expr(first_token(args)[0])
            line = "%s = %s" % (label, value)
            if config_file or label.upper() in CONFIG_SYMBOLS or label.upper().startswith("FEAT_"):
                lines = [".ifndef %s" % label, line, ".endif"]
            else:
                lines = [line]
        elif low == "org":
            target, _ = first_token(args)
            lines = labelled(["\tORG\t" + expr(target)])
        elif low in ("ift", "if"):
            lines = labelled([".if " + expr(args.strip())])
        elif low in ("eli", "elseif"):
            lines = labelled([".elseif " + expr(args.strip())])
        elif low in ("els", "else"):
            lines = labelled([".else"])
        elif low in ("eif", "endif"):
            lines = labelled([".endif"])
        elif low == "ert":
            e = expr(args.strip())
            lines = labelled(['\t.assert !(%s), error, "ERT %s"' % (e, args.strip().replace('"', "'"))])
        elif low == "icl":
            name, _ = first_token(args)
            lines = labelled(['\t.include "%s"' % self.out_name(name.strip("\"'"))])
        elif low == "ins":
            parts = split_list(first_token(args)[0])
            name = parts[0].strip("\"'")
            lines = labelled(['\t.incbin "%s"%s' % (name, "".join(", " + expr(p) for p in parts[1:]))])
        elif low == "opt":
            flags, _ = first_token(args)
            body = []
            for flag in re.findall(r"[a-zA-Z][+-]", flags):
                if flag[0].lower() == "h":
                    body.append("\tOPT_H\t%d" % (flag[1] == "+"))
                else:
                    body.append("; opt %s: no ca65 equivalent needed" % flag)
            lines = labelled(body)
        elif low == "run":
            target, _ = first_token(args)
            lines = labelled(["\tRUN\t" + expr(target)])
        elif low == "ini":
            target, _ = first_token(args)
            lines = labelled(["\tINI\t" + expr(target)])
        elif low == "dta":
            # the arguments end at the first blank outside quotes and brackets (the rest is a comment in MADS)
            data, _ = first_token(args)
            lines = labelled(dta(data))
        elif mnemonic == "=":
            lines = ["%s = %s" % (label, expr(args.strip()))]
        else:
            lines = labelled(instruction(mnemonic, args))

        if repeat is not None:
            lines = ["\t.repeat %d, %s" % (repeat, REPEAT_COUNTER)] + lines + ["\t.endrep"]
        if repeat is not None or pseudo_instruction(mnemonic):
            # the MADS line, before the instructions made of it
            lines.insert(0, ("\t; " if raw[0] in " \t" else "; ") + code.strip() + ("\t" + comment if comment else ""))
            return lines
        if comment:
            # the comment goes on the line (or alone before a translation of more lines)
            if len(lines) == 1:
                lines[0] += "\t" + comment
            else:
                lines.insert(0, "\t" + comment if raw[0] in " \t" else comment)
        return lines


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "asm"
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(src, "ca65")
    conv = Converter(src, dst)
    count = 0
    for root, dirs, files in os.walk(src):
        if os.path.abspath(root).startswith(os.path.abspath(dst)):
            continue
        for name in sorted(files):
            if re.search(r"\.(a65|asm)$", name, re.I):
                conv.convert_file(os.path.join(root, name))
                count += 1
    print("mads2ca65: %d files -> %s" % (count, dst))


if __name__ == "__main__":
    main()
