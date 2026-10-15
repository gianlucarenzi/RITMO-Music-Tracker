#pragma once

#include "PlatformTypes.h"
#include "Memory.h"
#include "tracker_obx.h"

// Which of the Patch 16 player's AUDF tables a note is read from, and where that table is (ported from RMT 1.36,
// plan 39, d81d88a and ffae5e7).
//
// The player chooses in two steps (asm/Patch-16/rmtplayr.a65): its TABLES_MSB dispatch table maps the distortion to
// one of the five 256-byte pages, then store_tables_lsb picks a 64-byte slice by the channel's clock - 64 kHz at
// +$00, 1.79 MHz at +$40, 16-bit at +$80 - while check_15khz takes the two 15 kHz tables from PAGE_EXTRA_0 instead.
//
// The page is READ FROM THE DRIVER IN MEMORY rather than written down here: a private copy of the layout is how this
// went wrong twice, GetFrequency() with the RMT 1.28 offsets and the MIDI input with the RMT 1.34 addresses.
class CFrequencyTables {
public:
    enum class Mode {
        Clock64kHz,
        Clock179MHz,
        Clock15kHz,
        Join16Bit,
    };

    /// <summary> The address of the AUDF table the player reads for this distortion and mode </summary>
    /// <param name="memory"> the emulated Atari's memory, with a Patch 16 driver loaded </param>
    /// <param name="distortion"> the instrument's RMT distortion, $00 to $0E </param>
    /// <returns> The table's address, or 0 when the loaded driver has no such table. The table holds 64 bytes, in
    /// Join16Bit mode 64 little-endian words. </returns>
    static MemoryAddress GetTableAddress(const byte* memory, int distortion, Mode mode);

    /// <summary> Which clock or width a channel plays at, from the AUDCTL the driver is about to write </summary>
    /// <param name="audctl"> the AUDCTL value, as in the driver's v_audctl </param>
    /// <param name="track"> the POKEY channel, 0-3 for the first chip and 4-7 for the second </param>
    /// <remarks> Only channels 1 and 3 of each chip can run at 1.79 MHz (tracks 0/4 and 2/6), and a 16-bit pair is
    /// addressed through its high byte, the channel above (tracks 1/5 and 3/7). 1.79 MHz and 16-bit both override the
    /// 15 kHz clock, as they do in the player. </remarks>
    static Mode GetModeForChannel(int audctl, int track);

    /// <summary> The AUDC distortion bits the player writes to POKEY for this distortion </summary>
    /// <returns> The AUDC value, or -1 when the loaded driver has no dispatch table. Read from that table, because it
    /// is not simply the distortion shifted up. </returns>
    static int GetDistortionAudc(const byte* memory, int distortion);
};
