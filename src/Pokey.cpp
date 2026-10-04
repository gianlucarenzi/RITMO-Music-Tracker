#include "PlatformTypes.h"
#include "Pokey.h"
#include "Atari.h"
#include "emu/PokeySound.h"

CPokey::CPokey()
{
    m_loaded = false;

    m_initialized = false;
    m_ntsc = false;
    m_stereo = false;
}

CPokey::~CPokey()
{
    DeInitSound();
}


void CPokey::InitSound()
{
    const char *name, *author, *description;
    RmtBuiltin_APokeySound_About(&name, &author, &description);
    m_about.Format("%s\n%s\n%s", name, author, description);
    m_loaded = true;
}

void CPokey::DeInitSound()
{
    m_loaded = false;
    m_about = "No POKEY emulation loaded";

    m_initialized = false;
    m_ntsc = false;
    m_stereo = false;
}

CString CPokey::GetAbout() const
{
    return m_about;
}

bool CPokey::IsSoundDriverLoaded() const
{
    return m_loaded;
}


void CPokey::InitPokeys(const bool ntsc, const bool stereo, const DWORD samplesPerSec)
{
    if (!m_loaded) return;

    if (!m_initialized || m_ntsc != ntsc || m_stereo != stereo || m_samplesPerSec != samplesPerSec) {
        RmtBuiltin_APokeySound_Initialize(stereo);
        RmtBuiltin_APokeySound_SetMainClock(CAtari::GetClockFrequency(ntsc));

        m_initialized = true;
        m_ntsc = ntsc;
        m_stereo = stereo;
        m_samplesPerSec = samplesPerSec;
    }
}

void CPokey::ResetPokeys()
{
    if (!m_loaded || !m_initialized) return;
    RmtBuiltin_APokeySound_Initialize(m_stereo);
}

void CPokey::PutByte(const byte address, const byte value)
{
    if (!m_loaded) return;
    RmtBuiltin_APokeySound_PutByte(address, value);
}
