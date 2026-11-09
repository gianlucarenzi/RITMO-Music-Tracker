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
#include "legacy_obx.h"
#include "emu/PokeySound.h"
#include "AtariBinaries.h"
#include "AtariIO.h"

extern CTuning g_Tuning;

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern CSong g_Song;
extern CXPokey g_Pokey;
extern CAtari g_Atari;
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
    g_AtariTrackerDriver->LoadRMTRoutines(g_defaultTrackerDriverVersion);
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
    if (const char* d = std::getenv("RMT_DRIVER")) g_defaultTrackerDriverVersion = (TrackerDriverVersion)std::atoi(d);
    g_Atari.Init(g_Song.IsNTSC());
    g_AtariTrackerDriver = new CAtariTrackerDriver(g_Atari);
    g_AtariTrackerDriver->LoadRMTRoutines(g_defaultTrackerDriverVersion);
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
            pokey.PutByte(0x0F, g_Atari.GetByteAt(0xd20f)); // SKCTL: the Two-Tone filter
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

// --twotone: the Two-Tone filter (SKCTL = $8B, the envelope's $7FE/$7FD
// command) must reach the sound. The song (Makary's example of RMT issue #30)
// is played by the tracker driver; the frames where it holds $8B in SKCTL are
// rendered twice by the built-in POKEY, with that SKCTL and with $03 (filter
// off): the samples must differ. Before, neither the live playback nor the
// POKEY emulation knew SKCTL, and the filter was silent.
static int TwoTone(const char* song)
{
    g_Atari.Init(g_Song.IsNTSC());
    g_AtariTrackerDriver = new CAtariTrackerDriver(g_Atari);
    g_AtariTrackerDriver->LoadRMTRoutines(TrackerDriverVersion::PATCH16);
    g_AtariTrackerDriver->Init();
    g_Song.ClearSong(8);
    if (!g_Song.FileOpen(song, FALSE)) {
        std::fprintf(stderr, "cannot open %s\n", song);
        return 1;
    }
    g_Pokey.InitSound(g_Song.IsNTSC(), g_Song.IsStereo());
    g_Song.Play(PLAY_SONG, FALSE, 0);
    g_Song.StopTimer();

    rmt_emu::PokeySound on, off;
    on.Initialize(false);
    off.Initialize(false);
    on.SetMainClock(g_Atari.GetClockFrequency());
    off.SetMainClock(g_Atari.GetClockFrequency());
    const int cyclesPerFrame = g_Atari.GetFrameCycleCount();
    int twoToneFrames = 0, differentFrames = 0;
    for (int f = 0; f < 50 * 20; f++) { // 20 s
        g_Song.TimerRoutine();
        const int skctl = g_Atari.GetByteAt(0xd20f);
        for (int i = 0; i < 9; i++) {
            on.PutByte(i, g_Atari.GetByteAt(0xd200 + i));
            off.PutByte(i, g_Atari.GetByteAt(0xd200 + i));
        }
        on.PutByte(0x0F, skctl);
        off.PutByte(0x0F, 0x03);
        uint8_t a[8192], b[8192];
        const int na = on.Generate(cyclesPerFrame, a, 16), nb = off.Generate(cyclesPerFrame, b, 16);
        if (skctl == 0x8B) {
            twoToneFrames++;
            if (na != nb || std::memcmp(a, b, na)) differentFrames++;
        }
    }
    std::printf("Two-Tone: %d frames with SKCTL $8B, %d of them sound different\n", twoToneFrames, differentFrames);
    return twoToneFrames > 0 && differentFrames > 0 ? 0 : 1;
}

// --tuning: the tuning of RMT 1.36 (plans 59 and 62). The tables of the default tuning are those the Patch16 driver
// ships, and PAL and NTSC generate the same ones (their A4 frequencies differ by exactly the clock ratio); the note A
// sounds at the A4 frequency in every key, for equal temperament, a preset and the custom temperament; and in equal
// temperament the tonic does not transpose: every key generates the tables of A-.
static int TuningTest()
{
    int failed = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
        if (!ok) failed++;
    };
    constexpr int TABLES = 0x600; // $B000-$B5FF
    auto tables = [](bool ntsc, const TTuningSettings& tuning) {
        g_Song.GetTuning() = tuning;
        g_Atari.Init(ntsc); // generates the tables with the clock of the region
        const byte* t = g_Atari.GetMemoryAt(RMT_FRQTABLES);
        return std::vector<byte>(t, t + TABLES);
    };
    TTuningSettings pal, ntsc;
    pal.Initialize(false);
    ntsc.Initialize(true);
    const std::vector<byte> palTables = tables(false, pal);
    check(palTables == tables(true, ntsc), "PAL and NTSC defaults generate the same tables");

    std::vector<byte> driver(0x10000, 0);
    byte* bin = nullptr;
    WORD size = 0;
    if (CRmtAtariBinaries::GetTrackerDriverBinary(TrackerDriverVersion::PATCH16, bin, size)) {
        MemoryAddress min, max;
        CAtariIO::LoadDataAsBinaryFile(bin, size, driver.data(), min, max);
    }
    // $B500-$B57F are the two hand-made sawtooth tables: not generated, they come with the driver
    int differ = 0;
    for (int i = 0; i < TABLES; i++) {
        if (i >= 0x500 && i < 0x580) continue;
        if (palTables[i] != driver[RMT_FRQTABLES + i]) {
            if (differ++ < 8) std::printf("     $%04X: generated $%02X, driver $%02X\n", RMT_FRQTABLES + i, palTables[i], driver[RMT_FRQTABLES + i]);
        }
    }
    check(differ == 0, "the default tuning generates the table page of the Patch16 driver (but the sawtooth tables)");

    // A at the A4 frequency (semitone 69: A of the octave the tables' formula puts A4 in) in every key
    TTuningSettings custom = pal;
    custom.temperament = CTuning::TUNING_CUSTOM;
    const double centsOfCustom[TTuningCents::COUNT] = { 0, 90, 192, 294, 390, 498, 588, 696, 792, 888, 996, 1092 };
    for (int i = 0; i < TTuningCents::COUNT; i++) custom.cents.values[i] = centsOfCustom[i];
    int wrong = 0;
    for (int temperament : { 0, 4, (int)CTuning::TUNING_CUSTOM }) {
        for (int tonic = 0; tonic < NOTES_PER_OCTAVE; tonic++) {
            TTuningSettings t = temperament == CTuning::TUNING_CUSTOM ? custom : pal;
            t.temperament = temperament;
            t.tonic = tonic;
            tables(false, t); // InitTuning() computes the custom multipliers
            const double a = g_Tuning.GetTruePitch(g_Tuning.CalculateTonicFrequency(t), temperament, tonic, 69);
            if (std::fabs(a - t.a4Frequency) > 1e-9 * t.a4Frequency) {
                std::printf("     temperament %d, tonic %d: A at %.9f Hz\n", temperament, tonic, a);
                wrong++;
            }
        }
    }
    check(wrong == 0, "A sounds at the A4 frequency in every key (equal, Werckmeister III, custom)");

    wrong = 0;
    for (int tonic = 0; tonic < NOTES_PER_OCTAVE; tonic++) {
        TTuningSettings t = pal;
        t.tonic = tonic;
        if (tables(false, t) != palTables) {
            std::printf("     tonic %d: other tables\n", tonic);
            wrong++;
        }
    }
    check(wrong == 0, "equal temperament generates the same tables in every key");

    g_Song.GetTuning() = pal;
    g_Atari.Init(false);
    std::printf("Tuning: %d check(s) wrong\n", failed);
    return failed ? 1 : 0;
}

// --midi16: the note input of MIDI channel 16 (the MIDI CH15 mode), which
// writes AUDF and AUDC straight into the driver's variables for rmt_set_pokey.
// For each distortion and clock (AUDCTL) a program change selects the
// distortion and a note is played; the AUDF must be the byte of the table
// the player itself reads (the addresses of tracker_obx.h and legacy_obx.h,
// generated from rmtplayr.a65 and rmt_ata.a65) and the AUDC must carry the
// player's distortion bits. Until RMT 1.36's fixes (plans 39 and 40) every
// Patch16 table but Poly5 and Distortion 4 was read a page too low, and the
// v1-family drivers played nothing.
struct TMidiNoteCase {
    const char* clock;
    int distortion, audctl, table, audc;
};

static int MidiChannel16Driver(TrackerDriverVersion version, const TMidiNoteCase* cases, int count, MemoryAddress trackAudf,
                               MemoryAddress trackAudc, MemoryAddress audctl)
{
    g_AtariTrackerDriver->LoadRMTRoutines(version);
    g_AtariTrackerDriver->Init();
    auto midi = [](int status, int d1, int d2) { g_Song.MidiEvent((DWORD)(status | d1 << 8 | d2 << 16), true); };
    byte* memory = g_Atari.GetMemoryAt(0);
    byte driverBefore[0x80]; // the driver's own data just above its track variables (v1: tabbeganddistor at $3182)
    std::memcpy(driverBefore, memory + 0x3180, sizeof(driverBefore));
    const int note = 0x18;
    int failed = 0;
    for (int c = 0; c < count; c++) {
        const TMidiNoteCase& t = cases[c];
        for (int i = 0; i < 16; i++) g_Midi.m_LastNoteOnChannel[i] = -1; // every channel free: the note goes to channel 1
        memory[audctl] = (byte)t.audctl;
        memory[trackAudf] = memory[trackAudc] = 0x55; // a marker the note must overwrite
        midi(0xCF, t.distortion / 2, 0);              // program change: the distortion
        midi(0x9F, note, 0x7F);                       // note on
        const int audf = memory[trackAudf], audc = memory[trackAudc];
        // the Patch16 track variables reach $3187: with a v1-family driver they are its v_aspeed and its tabbeganddistor
        const bool driverIntact = version == TrackerDriverVersion::PATCH16 || !std::memcmp(driverBefore, memory + 0x3180, sizeof(driverBefore));
        midi(0x8F, note, 0); // note off: the channel is free again
        const int wanted = memory[t.table + note];
        const bool ok = audf == wanted && (audc & 0xF0) == t.audc && driverIntact;
        if (!ok) failed++;
        std::printf("%-4s driver %d distortion %X %-8s AUDF $%02X (table $%04X: $%02X), AUDC $%02X (distortion bits $%02X)%s\n",
                    ok ? "ok" : "FAIL", (int)version, t.distortion, t.clock, audf, t.table + note, wanted, audc, t.audc,
                    driverIntact ? "" : ", the driver's data at $3180-$31FF overwritten");
        std::memcpy(memory + 0x3180, driverBefore, sizeof(driverBefore)); // the next note starts from the driver as loaded
    }
    return failed;
}

static int MidiChannel16()
{
    g_Atari.Init(false);
    g_AtariTrackerDriver = new CAtariTrackerDriver(g_Atari);
    g_AtariTrackerDriver->LoadRMTRoutines(TrackerDriverVersion::PATCH16);
    g_AtariTrackerDriver->Init();
    g_Song.ClearSong(8);
    g_prove = EditMode::MIDI_CH15_MODE;

    static const TMidiNoteCase patch16[] = {
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
    // the unpatched driver (asm/Legacy/rmt.a65, tabbeganddistor): one table per distortion, whatever the clock
    static const TMidiNoteCase unpatched[] = {
        { "64 kHz", 0x02, 0x00, RMT_ATA_FRQTABPURE, 0x20 },
        { "64 kHz", 0x0A, 0x00, RMT_ATA_FRQTABPURE, 0xA0 },
        { "64 kHz", 0x0C, 0x00, RMT_ATA_FRQTABBASS1, 0xC0 },
        { "64 kHz", 0x0E, 0x00, RMT_ATA_FRQTABBASS2, 0xC0 },
        { "15 kHz", 0x0A, 0x01, RMT_ATA_FRQTABPURE, 0xA0 },
    };
    const int count = (int)(sizeof(patch16) / sizeof(patch16[0]) + sizeof(unpatched) / sizeof(unpatched[0]));
    int failed = MidiChannel16Driver(TrackerDriverVersion::PATCH16, patch16, (int)(sizeof(patch16) / sizeof(patch16[0])),
                                     RMTPLAYR_TRACKN_AUDF, RMTPLAYR_TRACKN_AUDC, RMTPLAYR_V_AUDCTL);
    failed += MidiChannel16Driver(TrackerDriverVersion::UNPATCHED, unpatched, (int)(sizeof(unpatched) / sizeof(unpatched[0])),
                                  RMT_ATA_TRACKN_AUDF, RMT_ATA_TRACKN_AUDC, RMTPLAYR_V_AUDCTL);
    std::printf("MIDI channel 16: %d of %d checks wrong\n", failed, count);
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
    g_Song.GetTuning().Initialize(g_Song.IsNTSC());

    if (argc >= 5 && !std::strcmp(argv[1], "--play"))
        return PlaySong(argv[2], std::atoi(argv[3]), argv[4], argc >= 6 ? argv[5] : nullptr);
    if (argc >= 2 && !std::strcmp(argv[1], "--midi16")) return MidiChannel16();
    if (argc >= 3 && !std::strcmp(argv[1], "--twotone")) return TwoTone(argv[2]);
    if (argc >= 2 && !std::strcmp(argv[1], "--tuning")) return TuningTest();
    if (argc >= 3 && !std::strcmp(argv[1], "--screenshot"))
        return Screenshot(argv[2], 1280, 800, argc >= 4 ? argv[3] : nullptr);

    std::printf("Song name: '%s'\n", g_Song.GetName().GetString());
    std::printf("g_Atari and the tuning of g_Song initialised OK.\n");
    std::printf("RmtCoreTest finished successfully.\n");
    return 0;
}
