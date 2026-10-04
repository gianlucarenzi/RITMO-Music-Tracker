#include "SongTimer.h"


extern BOOL g_closeApplication; // Set when the application is busy shutting down


void CALLBACK TimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    ((CSongTimer*)dwUser)->Callback();
}

void CSongTimer::KillTimer()
{
    if (m_timerRoutine) {
        timeKillEvent(m_timerRoutine);
        m_timerRoutine = 0;
        m_song = nullptr;
    }
}

void CSongTimer::StopTimer()
{
    UINT timer;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_stopped = true; // A tick running now cannot set a new timer
        timer = m_timerRoutine;
        m_timerRoutine = 0;
    }
    if (timer) timeKillEvent(timer); // Outside the lock: it may wait for the running tick, which may be in SetTimer()
    while (busyInCallback) {};       // Make sure not in the timer handler
}

void CSongTimer::SetTimer(CSong& song, int ms)
{
    std::lock_guard<std::mutex> lock(m_lock);
    if (m_stopped) return;
    KillTimer();
    this->m_song = &song;
    m_timerRoutine = timeSetEvent(ms, 0, TimerCallback, (DWORD_PTR)(this), TIME_PERIODIC);
}

void CSongTimer::RestartTimer(CSong& song, int ms)
{
    std::lock_guard<std::mutex> lock(m_lock);
    m_stopped = false;
    KillTimer(); // none after StopTimer(), unless one was set since
    this->m_song = &song;
    m_timerRoutine = timeSetEvent(ms, 0, TimerCallback, (DWORD_PTR)(this), TIME_PERIODIC);
}

void CSongTimer::Callback()
{
    busyInCallback = true;
    m_song->TimerRoutine();
    m_timerRoutineProcessed = true; // TimerRoutine took place
    busyInCallback = false;
}
void CSongTimer::WaitForTimerRoutineProcessed()
{
    // If there is any timer at all
    if (m_timerRoutine) {
        m_timerRoutineProcessed = false;
        while (!m_timerRoutineProcessed && !g_closeApplication) {
            // Busy Waiting
        };
    }
}
