// CompatAudio.cpp - multimedia timers and DirectSound output (see CompatTypes.h)
//
// - timeSetEvent()/timeKillEvent(): one thread per timer. CSongTimer creates
//   a new timer from inside every tick (CSong::TimerRoutine -> ChangeTimer),
//   as on Windows; here a timer created from a tick of a timer with the same
//   callback starts at that timer's next deadline, so the song keeps its
//   tempo instead of drifting by the time the callback takes.
// - IDirectSoundBuffer: the secondary buffer of PokeyRenderer.cpp, a ring
//   played by PortAudio (RMT_HAVE_PORTAUDIO) with real cursors.

#include "PlatformTypes.h"
#include "Global.h" // g_audioBufferMs

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#ifdef RMT_HAVE_PORTAUDIO
#include <portaudio.h>
#ifdef __linux__
#include <dlfcn.h>
#endif
#endif

// ---------------------------------------------------------------------------
// timers
// ---------------------------------------------------------------------------

namespace {

// RMT_AUDIO_DEBUG=1: how often the audio server reported an underflow of the output stream (a click or a gap)
static std::atomic<int> s_outputUnderflows{ 0 };

using Clock = std::chrono::steady_clock;

struct MmTimer {
    std::thread thread;
    std::atomic<bool> stop{ false };
};

std::mutex s_timersLock;
std::map<UINT, std::shared_ptr<MmTimer>> s_timers;
UINT s_nextTimerId = 1;

// set on a timer thread while its callback runs
thread_local LPTIMECALLBACK t_callback = nullptr;
thread_local DWORD_PTR t_user = 0;
thread_local Clock::time_point t_deadline;
thread_local std::shared_future<void> t_callbackDone; // ready when the running callback has returned

// "too late" threshold: a timer this far behind restarts from now instead of catching up
constexpr std::chrono::milliseconds kMaxLateness(200);

} // namespace

UINT timeSetEvent(UINT delay, UINT, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags)
{
    if (!callback || !delay) return 0;
    Clock::time_point first = Clock::now() + std::chrono::milliseconds(delay);
    std::shared_future<void> after;
    if (t_callback == callback && t_user == user) {
        // rescheduled from its own tick: keep the tempo, but do not start the
        // callback before this one returns (else two ticks run concurrently,
        // and CSongTimer, which stores the new id after this call, ends up
        // with two live timers)
        first = t_deadline + std::chrono::milliseconds(delay);
        if (first < Clock::now() - kMaxLateness)
            first = Clock::now(); // too late: do not catch up
        after = t_callbackDone;
    }

    auto timer = std::make_shared<MmTimer>();
    UINT id;
    {
        std::lock_guard<std::mutex> lock(s_timersLock);
        id = s_nextTimerId++;
        if (!s_nextTimerId) s_nextTimerId = 1;
        s_timers[id] = timer;
    }
    timer->thread = std::thread([timer, id, delay, callback, user, flags, first, after] {
        if (after.valid()) after.wait();
        Clock::time_point deadline = first;
        while (!timer->stop) {
            std::this_thread::sleep_until(deadline);
            if (timer->stop) break;
            static const bool audioDebug = std::getenv("RMT_AUDIO_DEBUG") != nullptr; // RMT_AUDIO_DEBUG=1: timer and ring diagnostics on stderr
            if (audioDebug) {
                double late = std::chrono::duration<double, std::milli>(Clock::now() - deadline).count();
                if (late > 5) std::fprintf(stderr, "DBG timer late %.1f ms\n", late);
                static int reported = 0;
                if (s_outputUnderflows != reported) {
                    reported = s_outputUnderflows;
                    std::fprintf(stderr, "DBG audio output underflows so far: %d\n", reported);
                }
            }
            std::promise<void> done;
            t_callbackDone = done.get_future().share();
            t_callback = callback;
            t_user = user;
            t_deadline = deadline;
            callback(id, 0, user, 0, 0);
            t_callback = nullptr;
            t_callbackDone = {};
            done.set_value();
            if (!(flags & TIME_PERIODIC)) break;
            deadline += std::chrono::milliseconds(delay);
            if (deadline < Clock::now() - kMaxLateness)
                deadline = Clock::now(); // too late (debugger, suspend): do not catch up
        }
    });
    return id;
}

UINT timeKillEvent(UINT id)
{
    std::shared_ptr<MmTimer> timer;
    {
        std::lock_guard<std::mutex> lock(s_timersLock);
        auto it = s_timers.find(id);
        if (it == s_timers.end()) return 1; // MMSYSERR_ERROR
        timer = it->second;
        s_timers.erase(it);
    }
    timer->stop = true;
    if (timer->thread.get_id() == std::this_thread::get_id())
        timer->thread.detach(); // killed from its own callback
    else if (timer->thread.joinable())
        timer->thread.join();
    return 0;
}

// ---------------------------------------------------------------------------
// DirectSound buffer
// ---------------------------------------------------------------------------

bool g_rmtAudioOutput = true;

struct RmtAudioStream {
#ifdef RMT_HAVE_PORTAUDIO
    PaStream* stream = nullptr;
#endif
    DWORD safety = 0; // bytes between the play and the write cursor
    // test device (RMT_AUDIO_DUMP=file.wav): a thread takes the ring in real
    // time, like a sound card, and writes what it would play
    std::thread dumpThread;
    std::atomic<bool> dumpStop{ false };
    FILE* dumpFile = nullptr;
    uint32_t dumpBytes = 0;
};

// copy what the device plays next out of the ring, advancing the play cursor
static void TakeFromRing(IDirectSoundBuffer* buffer, unsigned char* out, DWORD bytes)
{
    DWORD size = (DWORD)buffer->m_data.size();
    DWORD pos = buffer->m_play;
    while (bytes) {
        DWORD n = std::min(bytes, size - pos);
        std::memcpy(out, buffer->m_data.data() + pos, n);
        out += n;
        bytes -= n;
        pos = (pos + n) % size;
    }
    buffer->m_play = pos;
}

static void WriteWavHeader(FILE* f, const WAVEFORMATEX& fmt, uint32_t dataBytes)
{
    auto put32 = [f](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto put16 = [f](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fseek(f, 0, SEEK_SET);
    std::fwrite("RIFF", 1, 4, f);
    put32(36 + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    put32(16);
    put16(1);
    put16(fmt.nChannels);
    put32(fmt.nSamplesPerSec);
    put32(fmt.nAvgBytesPerSec);
    put16(fmt.nBlockAlign);
    put16(fmt.wBitsPerSample);
    std::fwrite("data", 1, 4, f);
    put32(dataBytes);
}

static RmtAudioStream* StartDumpDevice(IDirectSoundBuffer* buffer, const char* path)
{
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::perror(path);
        return nullptr;
    }
    auto* s = new RmtAudioStream;
    s->dumpFile = f;
    WriteWavHeader(f, buffer->m_format, 0);
    const WAVEFORMATEX fmt = buffer->m_format;
    s->safety = fmt.nSamplesPerSec / 100 * fmt.nBlockAlign; // 10 ms
    s->dumpThread = std::thread([s, buffer, fmt] {
        auto start = Clock::now();
        uint64_t framesDone = 0;
        std::vector<unsigned char> chunk;
        while (!s->dumpStop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            double t = std::chrono::duration<double>(Clock::now() - start).count();
            uint64_t due = (uint64_t)(t * fmt.nSamplesPerSec);
            if (due <= framesDone) continue;
            chunk.resize((size_t)(due - framesDone) * fmt.nBlockAlign);
            TakeFromRing(buffer, chunk.data(), (DWORD)chunk.size());
            std::fwrite(chunk.data(), 1, chunk.size(), s->dumpFile);
            s->dumpBytes += (uint32_t)chunk.size();
            framesDone = due;
        }
    });
    return s;
}

IDirectSoundBuffer::IDirectSoundBuffer(DWORD bytes, const WAVEFORMATEX* format) : m_data(bytes)
{
    if (format) m_format = *format;
}

IDirectSoundBuffer::~IDirectSoundBuffer()
{
    Stop();
}

HRESULT IDirectSoundBuffer::Unlock(void* p1, DWORD s1, void*, DWORD s2)
{
    if (p1 && !m_data.empty())
        m_cursor = (DWORD)(((unsigned char*)p1 - m_data.data()) + s1 + s2) % (DWORD)m_data.size();
    return DS_OK;
}

#ifdef RMT_HAVE_PORTAUDIO
static int PlayCallback(const void*, void* output, unsigned long frames, const PaStreamCallbackTimeInfo*,
                        PaStreamCallbackFlags flags, void* user)
{
    if (flags & paOutputUnderflow) s_outputUnderflows++;
    auto* buffer = (IDirectSoundBuffer*)user;
    TakeFromRing(buffer, (unsigned char*)output, (DWORD)frames * buffer->m_format.nBlockAlign);
    return paContinue;
}

static bool PortAudioReady()
{
    static int state = 0; // 0 not tried, 1 ok, -1 failed
    if (!state) {
        state = Pa_Initialize() == paNoError ? 1 : -1;
        if (state > 0) std::atexit([] { Pa_Terminate(); });
    }
    return state > 0;
}

// paPulseAudio, not in the portaudio.h of PortAudio 19.7 and older
static const PaHostApiTypeId kPulseAudioHostApi = (PaHostApiTypeId)16;

static bool SameName(const char* a, const char* b)
{
    for (; *a && *b; a++, b++)
        if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
    return *a == *b;
}

// Whether the default device of ALSA goes to a sound server: it is then an
// ALSA plugin of type ioplug (pipewire of pipewire-alsa, pulse), not a sound
// card (hw, dmix, or plug in front of them). libasound is loaded here, so
// that RITMO does not need it to start.
static bool AlsaDefaultIsSoundServer()
{
#ifdef __linux__
    void* lib = dlopen("libasound.so.2", RTLD_NOW);
    if (!lib) return false;
    typedef int (*Open)(void**, const char*, int, int);
    typedef int (*Type)(void*);
    typedef int (*Close)(void*);
    auto open = (Open)dlsym(lib, "snd_pcm_open");
    auto type = (Type)dlsym(lib, "snd_pcm_type");
    auto close = (Close)dlsym(lib, "snd_pcm_close");
    bool server = false;
    void* pcm = nullptr;
    // SND_PCM_STREAM_PLAYBACK 0, SND_PCM_NONBLOCK 1, SND_PCM_TYPE_IOPLUG 28
    if (open && type && close && open(&pcm, "default", 0, 1) == 0) {
        server = type(pcm) == 28;
        close(pcm);
    }
    dlclose(lib);
    return server;
#else
    return false;
#endif
}

// The output device, so that RITMO is a client of the sound server
// (PulseAudio or PipeWire) like any other program when there is one: it
// plays on the output chosen in the desktop (Bluetooth headphones...) and
// follows its volume.
// - The default device of ALSA when it goes to the sound server
//   (pipewire-alsa, or the pulse plugin of ALSA): there moving the sound
//   between the outputs of the desktop works best.
// - Otherwise the default device of the PulseAudio host API, when there is
//   one (a PortAudio built with it and a PulseAudio or PipeWire server): on
//   a PipeWire without pipewire-alsa the default device of ALSA is a sound
//   card, and the sound would ignore the desktop.
// - Otherwise the default device of PortAudio, that of the first host API
//   (ALSA on Linux), where the configuration of ALSA says.
// RMT_AUDIO_HOST_API=<name>: the default device of another host API (ALSA,
// JACK, PulseAudio... as RMT_AUDIO_DEBUG=1 lists them)
static PaDeviceIndex OutputDevice()
{
    if (const char* name = std::getenv("RMT_AUDIO_HOST_API")) {
        for (PaHostApiIndex i = 0; i < Pa_GetHostApiCount(); i++) {
            const PaHostApiInfo* api = Pa_GetHostApiInfo(i);
            if (api && SameName(api->name, name) && api->defaultOutputDevice != paNoDevice)
                return api->defaultOutputDevice;
        }
        std::fprintf(stderr, "RMT: no audio output on the host API %s\n", name);
    }
    PaHostApiIndex alsa = Pa_HostApiTypeIdToHostApiIndex(paALSA);
    const PaHostApiInfo* api = alsa >= 0 ? Pa_GetHostApiInfo(alsa) : nullptr;
    if (api && api->defaultOutputDevice != paNoDevice && AlsaDefaultIsSoundServer()) {
        if (std::getenv("RMT_AUDIO_DEBUG"))
            std::fprintf(stderr, "DBG audio: the default device of ALSA goes to the sound server\n");
        return api->defaultOutputDevice;
    }
    PaHostApiIndex pulse = Pa_HostApiTypeIdToHostApiIndex(kPulseAudioHostApi);
    api = pulse >= 0 ? Pa_GetHostApiInfo(pulse) : nullptr;
    if (api && api->defaultOutputDevice != paNoDevice) return api->defaultOutputDevice;
    return Pa_GetDefaultOutputDevice();
}
#endif

HRESULT IDirectSoundBuffer::Play(DWORD, DWORD, DWORD)
{
    const char* dump = std::getenv("RMT_AUDIO_DUMP");
    if (g_rmtAudioOutput && !m_stream && !m_data.empty() && m_format.nBlockAlign && dump && *dump) {
        m_play = 0;
        m_stream = StartDumpDevice(this, dump);
        return DS_OK;
    }
#ifdef RMT_HAVE_PORTAUDIO
    const WAVEFORMATEX& f = m_format;
    if (!g_rmtAudioOutput || m_stream || m_data.empty() || !f.nChannels || !f.nSamplesPerSec || !PortAudioReady())
        return DS_OK;
    PaSampleFormat format = f.wBitsPerSample == 8 ? paUInt8 : paInt16;
    auto* s = new RmtAudioStream;
    // the callback size of the Options dialog (20 ms on Linux, 5 ms elsewhere)
    int bufferMs = g_audioBufferMs;
    // RMT_AUDIO_BUFFER_MS=<ms>: another size for this session only, without changing the settings
    if (const char* ms = std::getenv("RMT_AUDIO_BUFFER_MS")) {
        int value = std::atoi(ms);
        if (value >= 1) bufferMs = value;
    }
    bufferMs = std::max(1, std::min(bufferMs, RMT_MAX_AUDIO_BUFFER_MS));
    unsigned long framesPerBuffer = f.nSamplesPerSec / 1000 * bufferMs;
    PaStreamParameters out = {};
    out.device = OutputDevice();
    const PaDeviceInfo* outInfo = out.device != paNoDevice ? Pa_GetDeviceInfo(out.device) : nullptr;
    out.channelCount = f.nChannels;
    out.sampleFormat = format;
    // the latency of the device: at least 3 callbacks. The low latency of the
    // device (what Pa_OpenDefaultStream asks) is a single PipeWire quantum
    // (21 ms) through ALSA, and the output underflows at the least delay of
    // the sound server, whatever the callback size
    int latencyMs = 3 * bufferMs;
    // RMT_AUDIO_LATENCY_MS=<ms>: another latency for this session only
    if (const char* ms = std::getenv("RMT_AUDIO_LATENCY_MS")) {
        int value = std::atoi(ms);
        if (value >= 1) latencyMs = value;
    }
    latencyMs = std::min(latencyMs, RMT_MAX_AUDIO_LATENCY_MS);
    out.suggestedLatency = std::max(outInfo ? outInfo->defaultLowOutputLatency : 0.0, latencyMs / 1000.0);
    PaError err = out.device == paNoDevice ? paInvalidDevice
        : Pa_OpenStream(&s->stream, nullptr, &out, f.nSamplesPerSec, framesPerBuffer, paNoFlag, PlayCallback, this);
    // the sound server refuses the stream: the default device of PortAudio
    if (err != paNoError && out.device != Pa_GetDefaultOutputDevice() && Pa_GetDefaultOutputDevice() != paNoDevice) {
        std::fprintf(stderr, "RMT: cannot open the audio output (%s), trying the default one\n", Pa_GetErrorText(err));
        out.device = Pa_GetDefaultOutputDevice();
        outInfo = Pa_GetDeviceInfo(out.device);
        out.suggestedLatency = std::max(outInfo ? outInfo->defaultLowOutputLatency : 0.0, latencyMs / 1000.0);
        err = Pa_OpenStream(&s->stream, nullptr, &out, f.nSamplesPerSec, framesPerBuffer, paNoFlag, PlayCallback, this);
    }
    if (err != paNoError) {
        std::fprintf(stderr, "RMT: cannot open the audio output (PortAudio)\n");
        delete s;
        return DS_OK; // go on silent
    }
    const PaStreamInfo* info = Pa_GetStreamInfo(s->stream);
    // the PulseAudio host API gives as latency only that of its own buffer
    // (0 when the callbacks are as large as the pieces it sends), not that
    // of the sound server: there the latency asked is the best measure
    double latency = info ? std::max(info->outputLatency, out.suggestedLatency) : 0.02;
    if (std::getenv("RMT_AUDIO_DEBUG")) {
        for (PaHostApiIndex i = 0; i < Pa_GetHostApiCount(); i++) {
            const PaHostApiInfo* api = Pa_GetHostApiInfo(i);
            const PaDeviceInfo* device = api && api->defaultOutputDevice != paNoDevice ? Pa_GetDeviceInfo(api->defaultOutputDevice) : nullptr;
            std::fprintf(stderr, "DBG audio: host API %s: %d devices, default output %s\n", api ? api->name : "?",
                         api ? api->deviceCount : 0, device ? device->name : "none");
        }
        const PaDeviceInfo* device = Pa_GetDeviceInfo(out.device);
        const PaHostApiInfo* api = device ? Pa_GetHostApiInfo(device->hostApi) : nullptr;
        std::fprintf(stderr, "DBG audio: host API %s, device %s, %u Hz requested, %.0f Hz opened, %lu frames per callback, output latency %.1f ms\n",
                     api ? api->name : "?", device ? device->name : "?", (unsigned)f.nSamplesPerSec,
                     info ? info->sampleRate : 0.0, framesPerBuffer, latency * 1000);
    }
    // DirectSound's write cursor: where it is safe to write, past what the
    // device has already taken
    s->safety = ((DWORD)(latency * f.nSamplesPerSec) + framesPerBuffer) * f.nBlockAlign;
    m_play = 0;
    Pa_StartStream(s->stream);
    m_stream = s;
#endif
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
    if (m_stream && m_stream->dumpFile) {
        m_stream->dumpStop = true;
        m_stream->dumpThread.join();
        WriteWavHeader(m_stream->dumpFile, m_format, m_stream->dumpBytes);
        std::fclose(m_stream->dumpFile);
        delete m_stream;
        m_stream = nullptr;
        return DS_OK;
    }
#ifdef RMT_HAVE_PORTAUDIO
    if (m_stream) {
        Pa_StopStream(m_stream->stream);
        Pa_CloseStream(m_stream->stream);
        delete m_stream;
        m_stream = nullptr;
    }
#endif
    return DS_OK;
}

HRESULT IDirectSoundBuffer::GetCurrentPosition(DWORD* playCursor, DWORD* writeCursor)
{
    DWORD play, write;
    if (m_stream && !m_data.empty()) {
        play = m_play;
        write = (DWORD)((play + m_stream->safety) % m_data.size());
    } else {
        play = write = m_cursor; // instant mode
    }
    if (playCursor) *playCursor = play;
    if (writeCursor) *writeCursor = write;
    return DS_OK;
}
