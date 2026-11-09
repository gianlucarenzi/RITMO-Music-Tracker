// PokeySound.cpp - built-in POKEY sound for RMT (see PokeySound.h)

#include "PokeySound.h"

#include <cstring>

namespace rmt_emu {

static const unsigned POLY4_LEN = 15, POLY5_LEN = 31, POLY9_LEN = 511, POLY17_LEN = 131071;
static uint8_t s_poly4[POLY4_LEN], s_poly5[POLY5_LEN], s_poly9[POLY9_LEN], s_poly17[POLY17_LEN];

// maximal length LFSR x^n + x^(n-tap) + 1
static void MakePoly(uint8_t* dst, unsigned len, int bits, int tap)
{
    unsigned reg = 1;
    for (unsigned i = 0; i < len; i++) {
        dst[i] = reg & 1;
        reg = (reg >> 1) | (((reg ^ (reg >> tap)) & 1) << (bits - 1));
    }
}

static void InitPolys()
{
    static bool ready = false;
    if (ready) return;
    MakePoly(s_poly4, POLY4_LEN, 4, 1);
    MakePoly(s_poly5, POLY5_LEN, 5, 2);
    MakePoly(s_poly9, POLY9_LEN, 9, 4);
    MakePoly(s_poly17, POLY17_LEN, 17, 3);
    ready = true;
}

void PokeyChip::Reset()
{
    InitPolys();
    std::memset(m_audf, 0, sizeof(m_audf));
    std::memset(m_audc, 0, sizeof(m_audc));
    m_audctl = 0;
    m_skctl = 0x03; // serial and timers out of their init state
    for (int& c : m_counter) c = 1;
    std::memset(m_out, 0, sizeof(m_out));
    m_hp[0] = m_hp[1] = 0;
    m_poly4 = m_poly5 = m_poly9 = m_poly17 = 0;
    m_base = 28;
}

void PokeyChip::Write(int reg, uint8_t value)
{
    if (reg < 8) {
        if (reg & 1)
            m_audc[reg >> 1] = value;
        else
            m_audf[reg >> 1] = value;
    } else if (reg == 8) {
        m_audctl = value;
    } else if (reg == 0x0F) {
        m_skctl = value;
    }
}

uint8_t PokeyChip::Random() const
{
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) r |= (uint8_t)(s_poly17[(m_poly17 + i) % POLY17_LEN] << i);
    return r;
}

// a channel divider reached 0: AUDC bit 7 = 0 gates with poly5, bit 5 pure
// tone, else bit 6 poly4 or poly17/9; channels 3/4 clock the filters of 1/2
void PokeyChip::Pulse(int ch)
{
    uint8_t c = m_audc[ch];
    if ((c & 0x80) || s_poly5[m_poly5]) {
        if (c & 0x20)
            m_out[ch] ^= 1;
        else if (c & 0x40)
            m_out[ch] = s_poly4[m_poly4];
        else
            m_out[ch] = (m_audctl & 0x80) ? s_poly9[m_poly9] : s_poly17[m_poly17];
    }
    if (ch == 2 && (m_audctl & 0x04)) m_hp[0] = m_out[0];
    if (ch == 3 && (m_audctl & 0x02)) m_hp[1] = m_out[1];
}

void PokeyChip::ClockPair(int lo, int tick, uint8_t fast, uint8_t join)
{
    int hi = lo + 1;
    int clkLo = (m_audctl & fast) ? 1 : tick;
    if (m_audctl & join) {
        if (clkLo && --m_counter[hi] <= 0) {
            m_counter[hi] = m_audf[lo] + 256 * m_audf[hi] + ((m_audctl & fast) ? 7 : 1);
            Pulse(hi);
        }
        return;
    }
    // Two-tone mode (SKCTL bit 3, pair 1/2 only): channel 2 restarts the
    // divider of channel 1, and channel 1 that of channel 2 unless the break
    // is forced (bit 7). RMT's Two-Tone filter is SKCTL = $8B, as in ASAP.
    const bool twoTone = lo == 0 && (m_skctl & 0x08);
    if (clkLo && --m_counter[lo] <= 0) {
        m_counter[lo] = m_audf[lo] + ((m_audctl & fast) ? 4 : 1);
        Pulse(lo);
        if (twoTone && !(m_skctl & 0x80)) m_counter[hi] = m_audf[hi] + 1;
    }
    if (tick && --m_counter[hi] <= 0) {
        m_counter[hi] = m_audf[hi] + 1;
        Pulse(hi);
        if (twoTone) m_counter[lo] = m_audf[lo] + ((m_audctl & fast) ? 4 : 1);
    }
}

int PokeyChip::Cycle()
{
    if (++m_poly4 == POLY4_LEN) m_poly4 = 0;
    if (++m_poly5 == POLY5_LEN) m_poly5 = 0;
    if (++m_poly9 == POLY9_LEN) m_poly9 = 0;
    if (++m_poly17 == POLY17_LEN) m_poly17 = 0;

    int tick = 0;
    if (--m_base <= 0) {
        tick = 1;
        m_base = (m_audctl & 0x01) ? 114 : 28;
    }
    ClockPair(2, tick, 0x20, 0x08); // first: 3/4 latch the filters of 1/2
    ClockPair(0, tick, 0x40, 0x10);

    int sum = 0;
    for (int ch = 0; ch < 4; ch++) {
        uint8_t c = m_audc[ch];
        if (c & 0x10) {
            sum += c & 0x0F;
            continue;
        } // volume only
        int bit = m_out[ch];
        if (ch == 0 && (m_audctl & 0x04)) bit ^= m_hp[0];
        if (ch == 1 && (m_audctl & 0x02)) bit ^= m_hp[1];
        if (bit) sum += c & 0x0F;
    }
    return sum;
}

void PokeySound::Initialize(bool stereo)
{
    m_stereo = stereo;
    m_chip[0].Reset();
    m_chip[1].Reset();
    m_acc = 0;
    m_dc[0] = m_dc[1] = 0.0;
}

void PokeySound::PutByte(int addr, int data)
{
    int chip = (addr >> 4) & 1;
    if (chip && !m_stereo) return;
    m_chip[chip].Write(addr & 0x0F, (uint8_t)data);
}

int PokeySound::GetRandom(int addr) const
{
    return m_chip[(addr >> 4) & 1].Random();
}

// ASAP_FORMAT_U8 = 8, ASAP_FORMAT_S16_LE = 16, ASAP_FORMAT_S16_BE = -16
int PokeySound::Generate(int cycles, uint8_t* buffer, int format)
{
    int bytes = 0;
    m_acc += (long long)cycles * SAMPLE_RATE;
    long long samples = m_acc / m_clock;
    m_acc %= m_clock;
    long long cyc = 0;

    for (long long s = 0; s < samples; s++) {
        // the cycles of this sample (spread evenly over the call)
        long long end = (long long)cycles * (s + 1) / samples;
        int n = (int)(end - cyc);
        cyc = end;
        long sum0 = 0, sum1 = 0;
        for (int i = 0; i < n; i++) {
            sum0 += m_chip[0].Cycle();
            if (m_stereo) sum1 += m_chip[1].Cycle();
        }
        double v[2];
        v[0] = n ? (double)sum0 / n : 0.0;
        v[1] = m_stereo ? (n ? (double)sum1 / n : 0.0) : v[0];
        for (int ch = 0; ch < 2; ch++) {
            m_dc[ch] += (v[ch] - m_dc[ch]) * 0.0005; // POKEY output is 0..60: keep it centred
            double out = (v[ch] - m_dc[ch]) / 60.0;  // -1..1
            if (out > 1.0) out = 1.0;
            if (out < -1.0) out = -1.0;
            if (format == 8) {
                buffer[bytes++] = (uint8_t)(128 + (int)(out * 127.0));
            } else {
                int16_t w = (int16_t)(out * 32767.0);
                if (format == -16) {
                    buffer[bytes++] = (uint8_t)(w >> 8);
                    buffer[bytes++] = (uint8_t)w;
                } else {
                    buffer[bytes++] = (uint8_t)w;
                    buffer[bytes++] = (uint8_t)(w >> 8);
                }
            }
        }
    }
    return bytes;
}

} // namespace rmt_emu

// ---------------------------------------------------------------------------
// apokeysnd.dll entry points
// ---------------------------------------------------------------------------

static rmt_emu::PokeySound s_pokey;

void RmtBuiltin_APokeySound_Initialize(int stereo) { s_pokey.Initialize(stereo != 0); }
void RmtBuiltin_APokeySound_PutByte(int addr, int data) { s_pokey.PutByte(addr, data); }
int RmtBuiltin_APokeySound_GetRandom(int addr, int) { return s_pokey.GetRandom(addr); }
int RmtBuiltin_APokeySound_Generate(int cycles, unsigned char buffer[], int format) { return s_pokey.Generate(cycles, buffer, format); }
void RmtBuiltin_APokeySound_SetMainClock(int hz) { s_pokey.SetMainClock(hz); }

void RmtBuiltin_APokeySound_About(const char** name, const char** author, const char** description)
{
    *name = "RITMO built-in POKEY";
    *author = "RITMO";
    *description = "Cycle based POKEY sound, mono/stereo, 44100 Hz (the apokeysnd.dll of RMT is not needed)";
}
