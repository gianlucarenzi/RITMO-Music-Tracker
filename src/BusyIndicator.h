// BusyIndicator.h - a long piece of work (an export) tells the frontend that it is still busy.
//
// CExportSection (GuiHelpers.cpp) begins and ends it; the long loops (the song recorded into the POKEY stream, the
// LZSS compression, the WAV rendering) call BusyTick() as often as they like: it costs a check, and the Qt frontend
// shows a grey cover with a spinner over the window after the first 150 ms and keeps the spinner moving. Without a
// frontend (scripts, tests) it does nothing.

#pragma once

void BusyTick();
