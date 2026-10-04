/*
    The POKEY sound of the program: the built-in emulation of emu/PokeySound.
*/

#pragma once

typedef enum {
    ASAP_FORMAT_U8 = 8,      /* unsigned char */
    ASAP_FORMAT_S16_LE = 16, /* signed short, little-endian */
    ASAP_FORMAT_S16_BE = -16 /* signed short, big-endian */
} ASAP_SampleFormat;

class CPokey {
public:
    CPokey();
    ~CPokey();

    void InitSound();
    void DeInitSound();
    CString GetAbout() const;

    bool IsSoundDriverLoaded() const;

    void InitPokeys(const bool ntsc, const bool stereo, const DWORD samplesPerSec);
    void ResetPokeys(); // the emulation as just initialised: counters, polynomials, filters
    void PutByte(const byte address, const byte value);

private:
    bool m_loaded;
    CString m_about;

    bool m_initialized;
    bool m_ntsc;
    bool m_stereo;
    DWORD m_samplesPerSec;
};
