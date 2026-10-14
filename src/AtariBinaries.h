#pragma once

#include "PlatformTypes.h"

#include "TrackerDriverVersion.h"

class CRmtAtariBinaries {
public:
    // TODO Return CByteArray*
    // Have instance and free at end of application
    static bool GetTrackerDriverBinary(TrackerDriverVersion trackerDriverVersion, byte*& binary, WORD& size);
    static bool GetVUPlayerBinary(byte*& binary, WORD& size);

    // A player of resources/players (e.g. rmt_xex_v6_mono_pal.obx), nullptr if it is missing; the caller deletes it
    static CByteArray* LoadPlayerBinary(const CString& fileName);
};
