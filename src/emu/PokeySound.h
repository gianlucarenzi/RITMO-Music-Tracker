// PokeySound.h - built-in POKEY sound (one or two chips) for RITMO
//
// The POKEY of RITMO. It has the entry points of the apokeysnd.dll of RMT (ASAP),
// which it replaced:
// APokeySound_Initialize(stereo), APokeySound_PutByte(0..0x1F: 0x10.. is the
// second POKEY), APokeySound_GetRandom, APokeySound_Generate(cycles, buffer,
// format) and APokeySound_About. Generate() writes 44100 Hz samples, two
// interleaved channels (left = first POKEY, right = second POKEY, or the first
// on both sides in mono), which is what PokeyRenderer.cpp expects.
//
// The chip model is cycle based: 64 kHz / 15 kHz base clock, 1.79 MHz channels
// 1 and 3, 16 bit channel pairs, high-pass filters, poly4/poly5/poly9/poly17,
// the distortions of AUDC, volume-only mode and the two-tone mode of SKCTL. It is the POKEY emulation of
// AT2019/ATARI-Driver/RmtSkeleton/tools/rmtplay (checked against atari800).

#pragma once

#include <cstdint>

namespace rmt_emu {

class PokeyChip {
public:
    PokeyChip() { Reset(); }
    void Reset();
    void Write(int reg, uint8_t value); // 0..8 (AUDF/AUDC/AUDCTL), 0x0F (SKCTL: two-tone)
    int Cycle();                        // one machine cycle, returns the output level 0..60
    uint8_t Random() const;             // RANDOM ($D20A)

private:
    uint8_t m_audf[4], m_audc[4], m_audctl, m_skctl;
    int m_counter[4];
    uint8_t m_out[4], m_hp[2];
    unsigned m_poly4, m_poly5, m_poly9, m_poly17;
    int m_base;

    void Pulse(int ch);
    void ClockPair(int lo, int tick, uint8_t fast, uint8_t join);
};

class PokeySound {
public:
    static constexpr int SAMPLE_RATE = 44100;

    void Initialize(bool stereo);
    void SetMainClock(int hz) { m_clock = hz; } // PAL 1773447 (default), NTSC 1789790
    void PutByte(int addr, int data);
    int GetRandom(int addr) const;
    // cycles of the 6502 clock -> samples; returns the bytes written
    int Generate(int cycles, uint8_t* buffer, int format);

private:
    PokeyChip m_chip[2];
    bool m_stereo = false;
    int m_clock = 1773447;
    long long m_acc = 0;           // cycle / sample fraction
    double m_dc[2] = { 0.0, 0.0 }; // DC level removed from the output
};

} // namespace rmt_emu

// The apokeysnd.dll entry points, built in
void RmtBuiltin_APokeySound_Initialize(int stereo);
void RmtBuiltin_APokeySound_PutByte(int addr, int data);
int RmtBuiltin_APokeySound_GetRandom(int addr, int cycle);
int RmtBuiltin_APokeySound_Generate(int cycles, unsigned char buffer[], int format);
void RmtBuiltin_APokeySound_About(const char** name, const char** author, const char** description);
// not in apokeysnd.dll (always PAL there): the 6502 clock of the song
void RmtBuiltin_APokeySound_SetMainClock(int hz);
