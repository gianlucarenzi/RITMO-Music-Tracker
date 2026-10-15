#pragma once

#include "Atari.h"
#include "TrackerDriverVersion.h"

class CAtariTrackerDriver {
public:
    CAtariTrackerDriver(CAtari& atari);

    CAtari* GetAtari();

    int LoadRMTRoutines(const TrackerDriverVersion trackerDriverVersion);
    TrackerDriverVersion GetLoadedVersion() const { return m_loadedVersion; }
    // Whether the last LoadRMTRoutines() found its file and loaded it. False means the version asked for is not in
    // the emulated Atari - the one before it is, or none - which the caller reports instead of carrying on quietly.
    bool AreRoutinesLoaded() const { return m_routinesLoaded; }
    void UseVersion(const TrackerDriverVersion trackerDriverVersion, const bool ntsc);
    int Init();
    void Play();
    void SetPokey();
    void Silence();
    void SetTrackNoteInstrumentVolume(int t, int n, int i, int v);
    void SetTrackVolume(int t, int v);
    void InstrumentTurnOff(int instr);

    byte GetByteAt(const MemoryAddress address);

private:
    CAtari* m_atari;
    TrackerDriverVersion m_loadedVersion = NONE;
    bool m_routinesLoaded = false;
};
