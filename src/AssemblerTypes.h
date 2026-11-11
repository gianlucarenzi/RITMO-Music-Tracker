#pragma once

enum AssemblerFormat : int {
    ATASM = 0,
    XASM = 1,
    CA65 = 2 // the assembler of cc65: the syntax of ATASM, .proc for the local labels, .org (RITMO 2.8)
};
