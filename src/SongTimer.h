#pragma once

#include "PlatformTypes.h"

#include "Song.h"

#include <mutex>

class CSongTimer {
public:
    /// <summary>
    /// Immediately kill the timer event
    /// </summary>
    void KillTimer();

    /// <summary>
    /// Stop the timer and make sure that the timer event is not running
    /// </summary>
    void StopTimer();

    /// <summary>
    /// Set the timing of how often the CSong::TimerRoutine is being called.
    /// Depends on PAL or NTSC timing.
    /// </summary>
    /// <param name="ms">ms between calls (17=NTSC, 20=PAL)</param>
    void SetTimer(CSong& song, int ms);

    /// <summary>
    /// Start the timer again after StopTimer() (the end of an export): SetTimer()
    /// does nothing once the timer is stopped
    /// </summary>
    void RestartTimer(CSong& song, int ms);


    void Callback();

    /// <summary>
    /// Wait for the Timer Routine to run at least once
    /// </summary>
    void WaitForTimerRoutineProcessed();

private:
    CSong* m_song;
    UINT m_timerRoutine;
    bool volatile busyInCallback;
    bool volatile m_timerRoutineProcessed;

    // SetTimer() runs on the timer's own tick while StopTimer() runs on the
    // UI thread: the lock keeps them from both owning a live timer, and once
    // stopped no tick can start a new one
    std::mutex m_lock;
    bool m_stopped = false;
};
