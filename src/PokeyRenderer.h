//
// PokeyRender.h header file
//

#pragma once

#include "C6502.h"
#include "Pokey.h"

class CXPokey {
    // Construction
public:
    static constexpr size_t BUFFER_SIZE = 0x8000; // Must be a power of 2

    CXPokey();
    ~CXPokey();

    const CPokey* GetPokey() const;

    BOOL InitSound(const bool ntsc, const bool stereo);
    BOOL DeInitSound();
    BOOL ReInitSound(const bool ntsc, const bool stereo);

    const WAVEFORMATEX* GetSoundFormat() const;

    // Called by Song
    BOOL RenderSound1_50(int instrspeed);

    // Called by WaveFileExporter
    void RenderSoundV2(int instrspeed, BYTE* buffer, int& length);
    void RenderSoundV2Call(int instrspeed, BYTE* buffer, int& length); // one call of the driver: 1/instrspeed of a chunk
    void ResetRenderCalls() { m_renderCallRest = 0; }                  // before a series of calls
    void ResetPokeys() { m_pokey.ResetPokeys(); }                      // the same sound whatever played before


private:
    CPokey m_pokey;

    bool ntsc = false;
    bool stereo = false;

    int m_Latency = 0; // Chunks

    int m_ChunkSize = 0;
    C6502::ClockFrequency m_ClockFrequency = 0;
    C6502::CycleCount m_CyclesPerFrame = 0;
    float m_CyclesPerSample = 0;

    DWORD m_LoadPos = 0;
    WAVEFORMATEX m_SoundFormat{};
    DWORD m_LoadSize = 0;
    LPDIRECTSOUNDBUFFER m_SoundBuffer = nullptr;
    DWORD dwSize1 = 0, dwSize2 = 0;
    LPVOID Data1 = nullptr, Data2 = nullptr;
    BYTE m_PlayBuffer[BUFFER_SIZE] = {}; // Rendered part of the swing CHUNK_SIZE +- something (but it can be much bigger)
    DWORD m_PlayCursor = 0;
    DWORD m_WriteCursor = 0;
    DWORD m_WriteCursorStart = 0;

    int RenderPartV2(int renderpartsize, BYTE* buffer);
    int m_renderCallRest = 0; // the bytes RenderSoundV2Call() has not yet rendered of its chunks, times the instrument speed
    static int GetFrameRate(bool ntsc);
    static int GetCyclesPerFrame(bool ntsc);

    BOOL InitSoundInternal(const bool ntsc, const bool stereo, const WORD channels, const DWORD samplesPerSec, const WORD bitsPerSample);

    WORD GetChannels() const;
    int GetChunkSize() const;
    int GetLatencySize() const;

    bool IsSoundDriverLoaded() const;

    void CopyAtariMemoryToPokey();
};