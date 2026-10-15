#pragma once

#include "PlatformTypes.h"

#include "TrackerDriverVersion.h"

class CRmtAtariBinaries {
public:
    // TODO Return CByteArray*
    // Have instance and free at end of application
    static bool GetTrackerDriverBinary(TrackerDriverVersion trackerDriverVersion, byte*& binary, WORD& size);
    static bool GetVUPlayerBinary(byte*& binary, WORD& size);

    // The file a driver version is read from: named when it cannot be loaded
    static CString GetTrackerDriverFilePath(TrackerDriverVersion trackerDriverVersion);

    // What to say when that file is not there (ported from RMT 1.36, 19691e2): without it the driver of that version
    // is not in the emulated Atari, so it plays and exports with another one, or silence
    static CString GetMissingTrackerDriverMessage(TrackerDriverVersion trackerDriverVersion);

    // A player of resources/players (e.g. rmt_xex_v6_mono_pal.obx), nullptr if it is missing; the caller deletes it
    static CByteArray* LoadPlayerBinary(const CString& fileName);
};
