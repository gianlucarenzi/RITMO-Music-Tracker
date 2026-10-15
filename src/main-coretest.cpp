// main-coretest.cpp
//
// Smoke test for the "RmtCoreTest" target: links the whole RMT engine
// (Song/Tracks/Instruments/C6502/Pokey/IO_*/ASM*/SAPFile*/GUI_Song/
// GUI_Instruments/GuiHelpers/Global/TracksControl/ChannelControl/...)
// without the Qt frontend, on plain GCC.
//
// This intentionally does not exercise any UI - there is none yet, that's
// a later phase of the plan. It only needs to prove that the engine
// initialises its core global state without crashing.

#include "PlatformTypes.h"
#include "Global.h"
#include "Song.h"
#include "Tracks.h"
#include "Instruments.h"
#include "Tuning.h"
#include "resource.h"
#include "AtariTrackerDriver.h"
#include "PokeyRenderer.h"
#include "RmtMidi.h"
#include "tracker_obx.h"
#include "emu/PokeySound.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern CSong g_Song;
extern CXPokey g_Pokey;
extern CAtari g_Atari;
extern TTuningSettings g_tuning;
extern TTuningRatios g_tuningRatios;
extern CRmtMidi g_Midi;

// --screenshot: draw the main screen like CRmtView::DrawAll() into the
// software CDC of CompatTypes.h and write it as a PPM image
static int Screenshot(const char* out, int w, int h, const char* song)
{
    CBitmap gfxBitmap;
    if (!gfxBitmap.LoadBitmap(MAKEINTRESOURCE(IDB_GFX))) {
        std::fprintf(stderr, "cannot load the IDB_GFX bitmap\n");
        return 1;
    }
    CDC gfxDC;
    gfxDC.CreateCompatibleDC(nullptr);
    gfxDC.SelectObject(&gfxBitmap);
    g_gfx_dc = &gfxDC;

    // CRmtView::Resize() at 100% scaling
    g_width = w;
    g_height = h;
    g_tracklines = (g_height - (CSongScreenLayout::TRACKS_Y + 3 * 16) - 40) / 16;
    g_line_y = g_tracklines / 2;

    CBitmap memBitmap;
    memBitmap.Create(w, h);
    CDC memDC;
    memDC.CreateCompatibleDC(nullptr);
    memDC.SelectObject(&memBitmap);
    CPen pen(PS_SOLID, 1, CRGBColor::LINES);
    memDC.SelectObject(&pen);
    g_mem_dc = &memDC;

    // CRmtApp::InitInstance(): tracker driver, empty song
    g_Atari.Init(g_Song.IsNTSC());
    g_AtariTrackerDriver = new CAtariTrackerDriver(g_Atari);
    g_AtariTrackerDriver->LoadRMTRoutines(g_trackerDriverVersion);
    g_AtariTrackerDriver->Init();
    g_Song.ClearSong(8);

    g_activepart = g_active_ti = Part::PART_TRACKS;
    if (song && !g_Song.FileOpen(song, FALSE)) {
        std::fprintf(stderr, "cannot open %s\n", song);
        return 1;
    }

    // CRmtView::DrawAll()
    g_Song.RespectBoundaries();
    memDC.FillSolidRect(0, 0, w, h, CRGBColor::BACKGROUND);
    g_Song.DrawInfo();
    g_Song.DrawSong();
    g_Song.DrawAnalyzer();
    g_Song.DrawPlayTimeCounter();
    if (g_active_ti == Part::PART_TRACKS)
        g_Song.DrawTracks();
    else
        g_Song.DrawInstrument();

    FILE* f = std::fopen(out, "wb");
    if (!f) {
        std::perror(out);
        return 1;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = memBitmap.Bits()[i];
        unsigned char rgb[3] = { (unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::printf("Screenshot %dx%d written to %s\n", w, h, out);
    return 0;
}

// --play: play a song with the engine (tracker driver on the built-in 6502,
// one CSong::TimerRoutine() per frame) and write the POKEY registers the
// driver leaves in memory each frame; with a WAV name, also the sound of
// the built-in POKEY
static int PlaySong(const char* song, int frames, const char* regsOut, const char* wavOut)
{
    // RMT_DRIVER=1..7: the tracker driver of the Options dialog (TrackerDriverVersion)
    if (const char* d = std::getenv("RMT_DRIVER")) g_trackerDriverVersion = (TrackerDriverVersion)std::atoi(d);
    g_Atari.Init(g_Song.IsNTSC());
    g_AtariTrackerDriver = new CAtariTrackerDriver(g_Atari);
    g_AtariTrackerDriver->LoadRMTRoutines(g_trackerDriverVersion);
    g_AtariTrackerDriver->Init();
    g_Song.ClearSong(8);
    if (!g_Song.FileOpen(song, FALSE)) {
        std::fprintf(stderr, "cannot open %s\n", song);
        return 1;
    }
    g_Pokey.InitSound(g_Song.IsNTSC(), g_Song.IsStereo());
    g_Song.Play(PLAY_SONG, FALSE, 0);
    // the frames are played by the loop below, one TimerRoutine() each: the
    // timer of the song must not run it too (two threads in the engine, and a
    // deadlock when both change the timer)
    g_Song.StopTimer();

    FILE* regs = std::fopen(regsOut, "w");
    if (!regs) {
        std::perror(regsOut);
        return 1;
    }
    FILE* wav = wavOut ? std::fopen(wavOut, "wb") : nullptr;
    rmt_emu::PokeySound pokey;
    pokey.Initialize(false);
    pokey.SetMainClock(g_Atari.GetClockFrequency());
    std::vector<uint8_t> pcm;
    int cyclesPerFrame = g_Atari.GetFrameCycleCount();
    for (int f = 0; f < frames; f++) {
        g_Song.TimerRoutine();
        std::fprintf(regs, "%6d ", f);
        for (int i = 0; i < 9; i++) std::fprintf(regs, " %02X", g_Atari.GetByteAt(0xd200 + i));
        std::fprintf(regs, "\n");
        if (wav) {
            for (int i = 0; i < 9; i++) pokey.PutByte(i, g_Atari.GetByteAt(0xd200 + i));
            uint8_t buf[8192];
            int n = pokey.Generate(cyclesPerFrame, buf, 16);
            pcm.insert(pcm.end(), buf, buf + n);
        }
    }
    std::fclose(regs);
    if (wav) {
        // a WAV file is little endian, whatever the byte order of the machine
        auto put32 = [&](uint32_t v) {
            uint8_t b[4] = { uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24) };
            std::fwrite(b, 1, 4, wav);
        };
        auto put16 = [&](uint16_t v) {
            uint8_t b[2] = { uint8_t(v), uint8_t(v >> 8) };
            std::fwrite(b, 1, 2, wav);
        };
        std::fwrite("RIFF", 1, 4, wav);
        put32(36 + (uint32_t)pcm.size());
        std::fwrite("WAVEfmt ", 1, 8, wav);
        put32(16);
        put16(1);
        put16(2);
        put32(44100);
        put32(44100 * 4);
        put16(4);
        put16(16);
        std::fwrite("data", 1, 4, wav);
        put32((uint32_t)pcm.size());
        std::fwrite(pcm.data(), 1, pcm.size(), wav);
        std::fclose(wav);
    }
    std::printf("Played %d frames of %s\n", frames, song);
    return 0;
}

// --midi16: the note input of MIDI channel 16 (the MIDI CH15 mode), which
// writes AUDF and AUDC straight into the driver's variables for SetPokey.
// For each distortion and clock (AUDCTL) a program change selects the
// distortion and a note is played; the AUDF must be the byte of the table
// the player itself reads (the addresses of tracker_obx.h, generated from
// rmtplayr.a65) and the AUDC must carry the player's distortion bits. Until
// RMT 1.36's fix (plan 39) every table but Poly5 and Distortion 4 was read
// a page too low.
static int MidiChannel16()
{
    g_Atari.Init(false);
    g_AtariTrackerDriver = new CAtariTrackerDriver(g_Atari);
    g_AtariTrackerDriver->LoadRMTRoutines(TrackerDriverVersion::PATCH16);
    g_AtariTrackerDriver->Init();
    g_Song.ClearSong(8);
    g_prove = EditMode::MIDI_CH15_MODE;

    struct Case {
        const char* clock;
        int distortion, audctl, table, audc;
    };
    static const Case cases[] = {
        { "64 kHz", 0x02, 0x00, RMTPLAYR_FRQTABPOLY5_64KHZ, 0x20 },
        { "64 kHz", 0x04, 0x00, RMTPLAYR_FRQTABDIST4_64KHZ, 0x40 },
        { "64 kHz", 0x08, 0x00, RMTPLAYR_FRQTABPURE_64KHZ, 0x80 },
        { "64 kHz", 0x0A, 0x00, RMTPLAYR_FRQTABPURE_64KHZ, 0xA0 },
        { "64 kHz", 0x0C, 0x00, RMTPLAYR_FRQTABBUZZY_64KHZ, 0xC0 },
        { "64 kHz", 0x0E, 0x00, RMTPLAYR_FRQTABGRITTY_64KHZ, 0xC0 },
        { "15 kHz", 0x0A, 0x01, RMTPLAYR_FRQTABPURE_15KHZ, 0xA0 },
        { "15 kHz", 0x0C, 0x01, RMTPLAYR_FRQTABBUZZY_15KHZ, 0xC0 },
        { "1.79 MHz", 0x0A, 0x40, RMTPLAYR_FRQTABPURE_179MHZ, 0xA0 },
        { "1.79 MHz", 0x0C, 0x40, RMTPLAYR_FRQTABBUZZY_179MHZ, 0xC0 },
    };
    auto midi = [](int status, int d1, int d2) { g_Song.MidiEvent((DWORD)(status | d1 << 8 | d2 << 16), true); };
    byte* memory = g_Atari.GetMemoryAt(0);
    const int note = 0x18;
    int failed = 0;
    for (const Case& c : cases) {
        for (int i = 0; i < 16; i++) g_Midi.m_LastNoteOnChannel[i] = -1; // every channel free: the note goes to channel 1
        memory[RMTPLAYR_V_AUDCTL] = (byte)c.audctl;
        memory[RMTPLAYR_TRACKN_AUDF] = memory[RMTPLAYR_TRACKN_AUDC] = 0x55; // a marker the note must overwrite
        midi(0xCF, c.distortion / 2, 0);                                   // program change: the distortion
        midi(0x9F, note, 0x7F);                                            // note on
        const int audf = memory[RMTPLAYR_TRACKN_AUDF], audc = memory[RMTPLAYR_TRACKN_AUDC];
        midi(0x8F, note, 0); // note off: the channel is free again
        const int wanted = memory[c.table + note];
        const bool ok = audf == wanted && (audc & 0xF0) == c.audc;
        if (!ok) failed++;
        std::printf("%-4s distortion %X %-8s AUDF $%02X (table $%04X: $%02X), AUDC $%02X (distortion bits $%02X)\n",
                    ok ? "ok" : "FAIL", c.distortion, c.clock, audf, c.table + note, wanted, audc, c.audc);
    }
    std::printf("MIDI channel 16: %d of %d notes wrong\n", failed, (int)(sizeof(cases) / sizeof(cases[0])));
    return failed ? 1 : 0;
}

int main(int argc, char** argv)
{
    g_rmtAudioOutput = false; // deterministic: the sound buffer plays "instantly"
    // program folder = folder of the executable: resources/drivers is there
    {
        std::error_code ec;
        auto exe = std::filesystem::canonical("/proc/self/exe", ec);
        std::filesystem::path dir = ec ? std::filesystem::path(argv[0]).parent_path() : exe.parent_path();
        SetProgramFolderPath(CString((dir.string() + "/").c_str()));
    }
    std::printf("RmtCoreTest - RITMO engine (no Qt)\n");
    std::printf("Version string: %s\n", g_app.GetVersionAndBuild().GetString());

    // The non-GUI part of the start-up: the 6502 emulation (the built-in one,
    // emu/Cpu6502.cpp) and the tuning tables.
    g_Atari.Init();
    g_tuning.Initialize(g_Song.IsNTSC());
    g_tuningRatios.Initialize();

    if (argc >= 5 && !std::strcmp(argv[1], "--play"))
        return PlaySong(argv[2], std::atoi(argv[3]), argv[4], argc >= 6 ? argv[5] : nullptr);
    if (argc >= 2 && !std::strcmp(argv[1], "--midi16")) return MidiChannel16();
    if (argc >= 3 && !std::strcmp(argv[1], "--screenshot"))
        return Screenshot(argv[2], 1280, 800, argc >= 4 ? argv[3] : nullptr);

    std::printf("Song name: '%s'\n", g_Song.GetName().GetString());
    std::printf("g_Atari / g_tuning / g_tuningRatios initialised OK.\n");
    std::printf("RmtCoreTest finished successfully.\n");
    return 0;
}
