#pragma once

#include "General.h" // KeyboardLayout

#include <string>

// The note, the hex digit and the keypad digit of a key; -1 when it has none
extern int NoteKey(int vk);
extern int NumbKey(int vk);
extern int Numblock09Key(int vk);

// The QWERTY key at the position of vk on the layout's keyboard, for keys that
// mean a position (the Pokey Explorer's): the three letter rows, the ISO key
// discounted; + and - (0xBB/0xBD) and everything else stay themselves.
extern int ToQwertyPosition(int vk, KeyboardLayout layout);

// The note keys of the three layouts as a Markdown document ("### QWERTY", a
// keyboard picture, a Note/Keys table, "### QWERTZ", ...) - written by the
// script command "dump notekeys <file>".
extern std::string NoteKeysTable();
