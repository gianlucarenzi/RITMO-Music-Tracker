#include "BusyIndicator.h"
#include "PlatformTypes.h"
#include "GuiHelpers.h"
#include "Song.h"
#include "Instruments.h"
#include "AtariTrackerDriver.h"
#include "PokeyStream.h"
#include "ChannelControl.h"

extern CAtariTrackerDriver* g_AtariTrackerDriver;
extern CInstruments g_Instruments;
extern BOOL volatile g_rmtroutine;
extern long g_playtime;


/// <summary>
/// Get the Pokey registers to be dumped to a stream buffer.
/// GUI is disabled but window messages are being pumped, so the screen is updated
/// </summary>
/// <returns></returns>
void CSong::DumpSongToPokeyStream(CPokeyStream& pokeyStream, PlayMode playMode, int songline, int trackline)
{
    CString statusBarLog;
    int savedSongActiveLine = m_songactiveline;
    int savedTrackActiveLine = m_trackactiveline;
    long savedPlayTime = g_playtime;

    Stop();                       // Make sure RMT is stopped
    g_AtariTrackerDriver->Init(); // Reset the RMT routines
    SetChannelOnOff(-1, 0);       // Switch all channels off

    // Activate stream recording mode.
    m_pokeyStream = &pokeyStream;
    m_pokeyStream->StartRecording(*this, g_AtariTrackerDriver);

    // Play song using the chosen playback parameters
    // If no argument was passed, Play from start will be assumed
    m_songactiveline = songline;
    m_trackactiveline = trackline;
    Play(playMode, m_followplay);

    // Wait in a tight loop pumping messages until the playback stops
    {
        // The SAP-R dumper is running during that time...
        DWORD lastStatusTick = GetTickCount();
        while (m_play != PLAY_STOP) {
            // 1 VBI of module playback
            PlayVBI();

            // Increment the timer shown during playback
            g_playtime++;

            // Multiple RMT routine calls will be processed if needed
            for (int i = 0; i < m_instrumentSpeed; i++) {
                // 1 VBI of RMT routine (for instruments)
                if (g_rmtroutine) {
                    g_AtariTrackerDriver->Play();
                }
                // Transfer from memory to POKEY buffer
                pokeyStream.Record();
            }

            BusyTick(); // the busy indicator of the window keeps moving

            // The number of frames dumped so far, a few times per second
            DWORD now = GetTickCount();
            if (now - lastStatusTick >= 250) {
                lastStatusTick = now;
                statusBarLog.Format("Generating Pokey stream, playing song in quick mode... %i frames recorded", pokeyStream.GetCurrentFrame());
                SetStatusBarText(statusBarLog);
            }
        }
        g_AtariTrackerDriver->Init(); // Reset the RMT routines

        // End playback now, the SAP-R data should have been dumped successfully!
        Stop();

        // Deactivate stream recording.
        m_pokeyStream = nullptr;

        // The dump leaves no traces: the cursor and the play time as before
        m_songactiveline = savedSongActiveLine;
        m_trackactiveline = savedTrackActiveLine;
        g_playtime = savedPlayTime;

        statusBarLog.Format("Done... %i frames recorded in total, Loop point found at frame %i", pokeyStream.GetCurrentFrame(), pokeyStream.GetFirstCountPoint());
        SetStatusBarText(statusBarLog);
    }
}
