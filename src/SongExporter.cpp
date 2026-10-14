#include "PlatformTypes.h"
#include "SongExporter.h"
#include <algorithm>
#include <iomanip>
#include <memory>
#include <vector>

#include "AtariIO.h"
#include "AtariBinaries.h"

#include "GuiHelpers.h"

#include "exportdlgs.h"
#include "SAPFileExportDialog.h"

#include "lzss_sap.h"
#include "LZSSFile.h"

#include "SAPFile.h"
#include "SAPFileExporter.h"

#include "VUPlayer.h"
#include "rmt_xex.h"
#include "Atari.h"
#include "Global.h"

#include "WaveFileExporter.h"


// TODO: Find a better place and name
CString g_rmtmsxtext;

void CXEXFile::InitFromSong(const CSong& song)
{
    memcpy(this->songname, song.GetName(), SONG_NAME_MAX_LEN);
    this->songname[SONG_NAME_MAX_LEN] = 0;
    this->currentTime = CTime::GetCurrentTime(); // Use song time
    this->isNTSC = song.IsNTSC();
    this->isStereo = song.IsStereo();
    this->instrspeed = song.GetInstrumentSpeed();
}

CSongExporter::CSongExporter()
{
}

void CSongExporter::StrToAtariVideo(char* txt, int count)
{
    char a;
    for (int i = 0; i < count; i++) {
        a = txt[i] & 0x7f;
        if (a < 32) {
            a = 0;
        } else {
            if (a < 96) { a -= 32; }
        }
        txt[i] = a;
    }
}


int CSongExporter::BruteforceOptimalLZSS(unsigned char* src, int srclen, unsigned char* dst)
{
    CString message;
    CCompressLzss lzssData;

    // Start from a high value to force the first pattern to be the best one
    int bestScore = 0xFFFFFF;
    int optimal = 0;
    int result;
    {
        for (int i = 0; i < SAPR_OPTIMISATIONS_COUNT; i++) {
            int bruteforced = lzssData.LZSS_SAP(src, srclen, dst, i);

            if (bruteforced < bestScore) {
                bestScore = bruteforced;
                optimal = i;
            }

            message.Format("Compressing %i bytes, bruteforcing optimisation pattern %i... Current best: %i bytes with optimisation pattern %i", srclen, i, bestScore, optimal);
            SetStatusBarText(message);
        }

        result = lzssData.LZSS_SAP(src, srclen, dst, optimal);

        // Bruteforcing was completed, display some stats
        message.Format("Done... %i bytes were shrunk to %i bytes using the optimisation pattern %i", srclen, bestScore, optimal);
        SetStatusBarText(message);
    }

    return result;
}

bool CSongExporter::ExportLZSS(CSongExport& songExport, std::ofstream& ou)
{
    const CPokeyStream& pokeyStream = songExport.GetPokeyStream();

    SetStatusBarText("Compressing data ...");

    const int frameSize = CLZSSFile::GetFrameSize(songExport.GetSong());

    // Now, create LZSS files using the SAP-R dump created earlier
    // Large and on the heap, as LZSS_SAP() writes as much as the compression
    // yields: a stereo song's full stream compressed into a 64K stack buffer
    // overran it.
    const size_t LZSS_BUFFER_SIZE = 0xFFFFF;
    std::vector<byte> compressedData(LZSS_BUFFER_SIZE);

    CCompressLzss lzssData;

    // TODO: add a Dialog box for proper standalone LZSS exports
    // This is a hacked up method that was added only out of necessity for a project making use of song sections separately
    // I refuse to touch RMT2LZSS ever again
    CString fn = songExport.GetFilePath();
    fn = fn.Left(fn.GetLength() - 5); // In order to keep the filename without the extention

    // Full tune playback up to its loop point
    int full = lzssData.LZSS_SAP(pokeyStream.GetConstStreamBuffer(), pokeyStream.GetFirstCountPoint() * frameSize, compressedData.data());
    if (full > 16) {
        //ou.open(fn + "_FULL.lzss", ios::binary);	// Create a new file for the Full section
        ou.write((char*)compressedData.data(), full); // Write the buffer contents to the export file
    }
    ou.close(); // Close the file, if successful, it should not be empty

    // Intro section playback, up to the start of the detected loop point
    int intro = lzssData.LZSS_SAP(pokeyStream.GetConstStreamBuffer(), pokeyStream.GetThirdCountPoint() * frameSize, compressedData.data());
    if (intro > 16) // TODO: Why 16?
    {
        ou.open(fn + "_INTRO.lzss", std::ios::binary); // Create a new file for the Intro section
        ou.write((char*)compressedData.data(), intro);        // Write the buffer contents to the export file
    }
    ou.close(); // Close the file, if successful, it should not be empty

    // Looped section playback, this part is virtually seamless to itself
    int loop = lzssData.LZSS_SAP(pokeyStream.GetConstStreamBuffer() + (pokeyStream.GetFirstCountPoint() * frameSize), pokeyStream.GetSecondCountPoint() * frameSize, compressedData.data());
    if (loop > 16) {
        ou.open(fn + "_LOOP.lzss", std::ios::binary); // Create a new file for the Loop section
        ou.write((char*)compressedData.data(), loop);        // Write the buffer contents to the export file
    }
    ou.close(); // Close the file, if successful, it should not be empty

    ClearStatusBar();

    return true;
}

bool CSongExporter::ExportCompactLZSS(CSongExport& songExport, std::ofstream& ou)
{
    // TODO: everything related to exporting the stream buffer into small files and compress them to LZSS
    SetStatusBarText("Compressing data ...");

    const int frameSize = CLZSSFile::GetFrameSize(songExport.GetSong());

    int indexToSongline = 0;
    const CPokeyStream& pokeyStream = songExport.GetPokeyStream();
    int songlineCount = pokeyStream.GetSonglineCount();

    // Since 0 is also a valid offset, the initial values are set to -1 to prevent conflicts
    int listOfMatches[256];
    memset(listOfMatches, -1, sizeof(listOfMatches));

    // Now, create LZSS files using the SAP-R dump created earlier
    unsigned char compressedData[RAM_SIZE];

    CCompressLzss lzssData;

    // TODO: add a Dialog box for proper standalone LZSS exports
    CString fn = songExport.GetFilePath();
    fn = fn.Left(fn.GetLength() - 5); // In order to keep the filename without the extention
    ou.close();

    // For all songlines to index, process with comparisons and find duplicates
    while (indexToSongline < songlineCount) {
        int bytesCount = pokeyStream.GetFramesPerSongline(indexToSongline) * frameSize;

        int index1 = pokeyStream.GetOffsetPerSongline(indexToSongline);
        const unsigned char* buff1 = pokeyStream.GetConstStreamBuffer() + (index1 * frameSize);

        // If there is no index already, assume the Index1 to be the first occurence
        if (listOfMatches[indexToSongline] == -1)
            listOfMatches[indexToSongline] = index1;

        // Compare all indexes available and overwrite matching songline streams with index1's offset
        for (int i = 0; i < songlineCount; i++) {
            // If the bytes count between 2 songlines isn't matching, don't even bother trying
            if (bytesCount != pokeyStream.GetFramesPerSongline(i) * frameSize)
                continue;

            int index2 = pokeyStream.GetOffsetPerSongline(i);
            const unsigned char* buff2 = pokeyStream.GetConstStreamBuffer() + (index2 * frameSize);

            // If there is a match, the second index will adopt the offset of the first index
            if (!memcmp(buff1, buff2, bytesCount))
                listOfMatches[i] = index1;
        }

        // Process to the next songline index until they are all processed
        indexToSongline++;
    }
    indexToSongline = 0;

    // From here, data blocs based on the Songline index and offset will be written to file
    // This should strip away every duplicated chunks, but save just enough data for reconstructing everything
    while (indexToSongline < songlineCount) {
        // Get the current index to Songline offset from the current position
        int index = listOfMatches[indexToSongline];

        // Find if this offset was already processed from a previous Songline
        for (int i = 0; i < songlineCount; i++) {
            // As soon as a match is found, increment the counter for how many times the index was referenced
            if (index == listOfMatches[i] && indexToSongline > i) {
                // I don't know anymore, at this point...
            }
        }

        // Process to the next songline index until they are all processed
        indexToSongline++;
    }

    // Create a new file for logging everything related to the procedure
    ou.open(fn + ".txt", std::ios::binary);
    ou << "This is a test that displays all duplicated SAP-R bytes from m_StreamBuffer." << std::endl;
    ou << "Each ones of the Buffer Chunks are indexed into memory using Songlines.\n"
       << std::endl;

    for (int i = 0; i < songlineCount; i++) {
        ou << "Index: " << PADHEX(2, i);
        ou << ",\t Offset (real): " << PADHEX(4, pokeyStream.GetOffsetPerSongline(i));
        ou << ",\t Offset (dupe): " << PADHEX(4, listOfMatches[i]);
        ou << ",\t Bytes (uncompressed): " << PADDEC(1, pokeyStream.GetFramesPerSongline(i) * frameSize);
        ou << ",\t Bytes (LZ16 compressed): " << PADDEC(1, lzssData.LZSS_SAP(pokeyStream.GetConstStreamBuffer() + (pokeyStream.GetOffsetPerSongline(i) * frameSize), pokeyStream.GetFramesPerSongline(i) * frameSize, compressedData));
        ou << std::endl;
    }

    ou.close();

    ClearStatusBar();

    return true;
}

bool CSongExporter::ExportSAP_R(CSongExport& songExport, std::ofstream& ou)
{
    CSAPFile sapFile;
    sapFile.SetType("R");
    if (!CSAPFileExportDialog::Show(songExport.GetSong(), sapFile)) {
        return false;
    };

    bool result = CSAPFileExporter::ExportSAP_R(songExport, sapFile, ou);
    return result;
}


bool CSongExporter::ExportSAP_B_LZSS(CSongExport& songExport, std::ofstream& ou)
{
    CSAPFile sapFile;
    sapFile.SetType("B");
    if (!CSAPFileExportDialog::Show(songExport.GetSong(), sapFile)) {
        return false;
    }

    bool result = CSAPFileExporter::ExportSAP_B_LZSS(songExport, sapFile, ou);
    return result;
}


bool CSongExporter::ExportWAV(CSongExport& songExport, std::ofstream& ou, CXPokey& pokey, byte* memory)
{
    return CWaveFileExporter::ExportWAV(songExport, ou, pokey, memory);
}

bool CSongExporter::ExportXEX_LZSS(CSongExport& songExport, std::ofstream& ou)
{
    // Create the export metadata for songname, Atari text, parameters, etc
    CXEXFile xexFile;

    if (!ShowXEXExportDialog(songExport.GetSong(), xexFile)) {
        return false;
    }
    return ExportXEX_LZSS(songExport, xexFile, ou);
}

bool CSongExporter::ExportXEX_LZSS(CSongExport& songExport, CXEXFile xexFile, std::ofstream& ou)
{
    CString s, t;

    int subsongs = songExport.GetSong().GetSubsongParts(t);

    int subtune[256]{};

    byte mem[RAM_SIZE]{}; // Default RAM size for most 800xl/xe machines

    // GetSubsongParts returns a CString, so the values must be converted back to int first, FIXME
    for (int i = 0; i < subsongs; i++) {
        char c[3]{ t[i * 3], t[i * 3 + 1], '\0' };
        subtune[i] = strtoul(c, NULL, 16);
    }

    // Load VUPlayerLZSS to memory
    MemoryAddress addressFrom, addressTo;
    WORD size;
    byte* bin;

    if (!CRmtAtariBinaries::GetVUPlayerBinary(bin, size)) {
        return false;
    }
    if (!CAtariIO::LoadDataAsBinaryFile(bin, size, mem, addressFrom, addressTo)) {
        return false;
    }


    int lzss_total = 0;
    int framescount = 0;
    if (!BuildLzssSubtunes(songExport, subtune, subsongs, mem, lzss_total, framescount)) {
        return false;
    }

    // Write the Atari Video text to memory, for 5 lines of 40 characters
    memcpy(&mem[LZSSP_LINE_1], xexFile.atariText, CXEXFile::ATARI_TEXT_SIZE);

    // Write the total framescount on the top line, next to the Region and VBI speed, for 28 characters
    memset(&mem[LZSSP_LINE_0 + 0x0B], 32, 28);
    char framesdisplay[28] = { 0 };
    sprintf(framesdisplay, "(%i frames total)", framescount);
    for (int i = 0; i < 28; i++) mem[LZSSP_LINE_0 + 0x0B + i] = framesdisplay[i];
    CSongExporter::StrToAtariVideo((char*)mem + LZSSP_LINE_0 + 0x0B, 28);

    // I know the binary I have is currently set to NTSC, so I'll just convert to PAL and keep this going for now...
    if (!xexFile.isNTSC) {
        // clang-format off
        unsigned char regionbytes[] = {
            0xB9,(LZSSP_TABPPPAL - 1) & 0xff,(LZSSP_TABPPPAL - 1) >> 8,         // LDA tabppPAL-1,y
            0x8D,LZSSP_ACPAPX2 & 0xFF,LZSSP_ACPAPX2 >> 8,                       // STA acpapx2
            0xE0,0x9B,                                                          // CPX #$9B
            0x30,0x05,                                                          // BMI set_ntsc
            0xB9,(LZSSP_TABPPPALFIX - 1) & 0xff,(LZSSP_TABPPPALFIX - 1) >> 8,   // LDA tabppPALfix-1,y
            0xD0,0x03,                                                          // BNE region_done
            0xB9,(LZSSP_TABPPNTSCFIX - 1) & 0xFF,(LZSSP_TABPPNTSCFIX - 1) >> 8  // LDA tabppNTSCfix-1,y
        };
        // clang-format on
        memcpy(&mem[VUPlayer::REGION], regionbytes, sizeof(regionbytes));
    }

    // Additional patches from the Export Dialog...
    mem[VUPlayer::SONG_SPEED] = xexFile.instrspeed;                       // Song speed
    mem[VUPlayer::RASTER_BAR] = xexFile.displayRasterbar ? 0x80 : 0x00;   // Display the rasterbar for CPU level
    mem[VUPlayer::COLOR] = xexFile.rasterbarColor;                        // Rasterbar colur
    mem[VUPlayer::STEREO_FLAG] = xexFile.isStereo ? 0xFF : 0x00;          // Is the song stereo?
    mem[VUPlayer::SONGTOTAL] = subsongs;                                  // Total number of subtunes
    if (!xexFile.autoRegion) {                                            // Automatically adjust speed between regions?
        for (int i = 0; i < 4; i++) mem[VUPlayer::REGION + 6 + i] = 0xEA; // set the 4 bytes to NOPs to disable it
    }

    // Reconstruct the export binary for the LZSS Driver, VUPlayer, and all the included data
    CAtariIO::SaveBinaryBlock(ou, mem, LZSSP_PLAYLZ16BEGIN, LZSSP_SONGINDEX, 1);

    // Set the run address to VUPlayer
    mem[0x2e0] = LZSSP_VUPLAYER & 0xff;
    mem[0x2e1] = LZSSP_VUPLAYER >> 8;
    CAtariIO::SaveBinaryBlock(ou, mem, 0x2e0, 0x2e1, 0);

    // Overwrite the LZSS data region with both the pointers for subtunes index, and the actual LZSS streams until the end of file
    CAtariIO::SaveBinaryBlock(ou, mem, VUPlayer::LZSS_POINTER, lzss_total, 0);

    return true;
}

void CRmtXexFile::InitFromSong(const CSong& song)
{
    instrspeed = song.GetInstrumentSpeed();
    isStereo = song.IsStereo();
    isNTSC = song.IsNTSC();

    CString text = song.GetName();
    text.Trim();
    if (text.IsEmpty() || text == "Noname song") {
        // a module without its text (a stripped module): its file name, as rmtinfo.py does
        CString file = song.GetFilename();
        int slash = std::max(file.ReverseFind('/'), file.ReverseFind('\\'));
        file = file.Mid(slash + 1);
        int dot = file.ReverseFind('.');
        text = dot > 0 ? file.Left(dot) : file;
    }
    int comma = text.Find(", ");
    if (comma >= 0) {
        name = text.Left(comma);
        author = text.Mid(comma + 2);
    } else {
        name = text;
        author = "";
    }
    name.Trim();
    author.Trim();
    // the first year of the text, unless the author line shows it already
    date = "";
    for (int i = 0; i + 4 <= text.GetLength(); i++) {
        CString year = text.Mid(i, 4);
        bool digits = true;
        for (int j = 0; j < 4; j++) digits &= isdigit((unsigned char)year.GetAt(j)) != 0;
        bool alone = (i == 0 || !isdigit((unsigned char)text.GetAt(i - 1))) && (i + 4 == text.GetLength() || !isdigit((unsigned char)text.GetAt(i + 4)));
        if (digits && alone && (year.Left(2) == "19" || year.Left(2) == "20")) {
            if (author.Find(year) < 0) {
                date = year;
            }
            break;
        }
    }
}

bool CSongExporter::ShowRmtXexExportDialog(const CSong& song, CRmtXexFile& xexFile)
{
    xexFile.InitFromSong(song);

    CSAPFileExportDialog dlg;
    dlg.m_title = "Export Atari executable with the RMT player";
    dlg.m_name = xexFile.name;
    dlg.m_author = xexFile.author;
    dlg.m_date = xexFile.date;
    dlg.m_noSubsongs = TRUE;
    if (dlg.DoModal() != IDOK) {
        return false;
    }
    xexFile.name = dlg.m_name;
    xexFile.author = dlg.m_author;
    xexFile.date = dlg.m_date;
    return true;
}

bool CSongExporter::ExportXEX_RMTPlayer(CSongExport& songExport, std::ofstream& ou)
{
    CRmtXexFile xexFile;

    if (!ShowRmtXexExportDialog(songExport.GetSong(), xexFile)) {
        return false;
    }
    return ExportXEX_RMTPlayer(songExport, xexFile, ou);
}

/// <summary>
/// The players are resources/players/rmtplay_v1/v6_mono/stereo.xex, built by asm/rmtplay (make install): the
/// visualizer of RMTPLAY (a cc65 program at $4000) followed by the RMT player of a driver at its own address, RMT 1.28
/// (v1, drivers Unpatched) at $3400 or Patch16 (v6) at $3100 with its frequency tables at $3A00, which RITMO computes
/// from the tuning: they are taken from the memory of the tracker driver. rmt_xex.h has the addresses patched here:
/// the module goes right after the program (RMTXEX_*_MODULE) up to RMTXEX_MODULE_LIMIT, rmt_song / rmt_song_end
/// point to it, rmt_song_ntsc says the region of the song, rmt_info holds NAME, AUTHOR and DATE.
/// </summary>
bool CSongExporter::ExportXEX_RMTPlayer(CSongExport& songExport, const CRmtXexFile& xexFile, std::ofstream& ou)
{
    // The driver the tracker plays the song with (the one of the Options, or the RMT 1.28 Unpatched one a song with
    // the Autofilter may be played with)
    bool patch16;
    switch (g_AtariTrackerDriver->GetLoadedVersion()) {
        case TrackerDriverVersion::UNPATCHED:
        case TrackerDriverVersion::UNPATCHED_WITH_TUNING:
            patch16 = false;
            break;
        case TrackerDriverVersion::PATCH16:
            patch16 = true;
            break;
        default:
            SendErrorMessage("XEX with the RMT player",
                             "There is an RMT player for the drivers RMT 1.28 Unpatched and RMT 1.34 Patch16 only\n"
                             "(Options > RMT Driver Version).\n\n"
                             "The XEX export with the LZSS driver plays the song with any driver.");
            return false;
    }
    if (xexFile.instrspeed < 1 || xexFile.instrspeed > 4) {
        SendErrorMessage("XEX with the RMT player", "The RMT player plays instrument speeds 1 to 4 only.");
        return false;
    }

    struct TVariant {
        const char* fileName;
        MemoryAddress module, song, info;
    };
    static const TVariant variants[4] = {
        { "rmtplay_v1_mono.xex", RMTXEX_V1_MONO_MODULE, RMTXEX_V1_MONO_RMT_SONG, RMTXEX_V1_MONO_RMT_INFO },
        { "rmtplay_v1_stereo.xex", RMTXEX_V1_STEREO_MODULE, RMTXEX_V1_STEREO_RMT_SONG, RMTXEX_V1_STEREO_RMT_INFO },
        { "rmtplay_v6_mono.xex", RMTXEX_V6_MONO_MODULE, RMTXEX_V6_MONO_RMT_SONG, RMTXEX_V6_MONO_RMT_INFO },
        { "rmtplay_v6_stereo.xex", RMTXEX_V6_STEREO_MODULE, RMTXEX_V6_STEREO_RMT_SONG, RMTXEX_V6_STEREO_RMT_INFO },
    };
    const TVariant& variant = variants[(patch16 ? 2 : 0) + (xexFile.isStereo ? 1 : 0)];

    std::unique_ptr<CByteArray> binary(CRmtAtariBinaries::LoadPlayerBinary(variant.fileName));
    if (!binary || binary->GetSize() < 6) {
        CString message;
        message.Format("Couldn't load the RMT player '%s'.", variant.fileName);
        SendErrorMessage("Export aborted", message);
        return false;
    }

    // The blocks of the program and of the player, in their order
    byte mem[RAM_SIZE]{};
    std::vector<std::pair<MemoryAddress, MemoryAddress>> blocks;
    const byte* data = binary->GetData();
    long size = binary->GetSize(), p = 0;
    while (p + 4 <= size) {
        if (data[p] == 0xFF && data[p + 1] == 0xFF) {
            p += 2;
            continue;
        }
        MemoryAddress from = data[p] | data[p + 1] << 8, to = data[p + 2] | data[p + 3] << 8;
        if (to < from || p + 4 + (to - from) >= size) {
            break;
        }
        memcpy(mem + from, data + p + 4, to - from + 1);
        blocks.push_back({ from, to });
        p += 4 + (to - from + 1);
    }
    auto loaded = [&](MemoryAddress a) {
        for (const auto& block : blocks)
            if (a >= block.first && a <= block.second) return true;
        return false;
    };
    if (!loaded(variant.song) || !loaded(variant.info + 3 * (RMTXEX_INFO_MAX + 1) - 1) || (patch16 && !loaded(RMTXEX_V6_MONO_TABLES))) {
        CString message;
        message.Format("The RMT player '%s' is not the one this program expects.", variant.fileName);
        SendErrorMessage("Export aborted", message);
        return false;
    }

    // The module, right after the program
    CSong& song = songExport.GetSong();
    TExportDescription module{};
    int moduleEnd = song.MakeModule(module.mem, variant.module, SongIOType::XEX_RMTPLAYER, module.instrumentSavedFlags, module.trackSavedFlags);
    if (moduleEnd < 0) {
        return false;
    }
    if (moduleEnd > RMTXEX_MODULE_LIMIT) {
        CString message;
        message.Format("Error, the module ($%04X - $%04X) is too big for the RMT player!\n\n"
                       "The player has room for a module of %d bytes ($%04X - $%04X).\n"
                       "The XEX export with the LZSS driver may fit it.",
                       variant.module, moduleEnd - 1, RMTXEX_MODULE_LIMIT - variant.module, variant.module, RMTXEX_MODULE_LIMIT - 1);
        SendErrorMessage("Buffer Overflow", message);
        return false;
    }

    // rmt_song, rmt_song_end, rmt_song_ntsc (rmtxex.s) and the info lines (rmtplay.c), ASCII, zero terminated
    mem[variant.song + 0] = variant.module & 0xFF;
    mem[variant.song + 1] = variant.module >> 8;
    mem[variant.song + 2] = moduleEnd & 0xFF;
    mem[variant.song + 3] = moduleEnd >> 8;
    mem[variant.song + 4] = xexFile.isNTSC ? 1 : 0;
    const CString* info[3] = { &xexFile.name, &xexFile.author, &xexFile.date };
    for (int i = 0; i < 3; i++) {
        byte* line = mem + variant.info + i * (RMTXEX_INFO_MAX + 1);
        memset(line, 0, RMTXEX_INFO_MAX + 1);
        for (int j = 0; j < info[i]->GetLength() && j < RMTXEX_INFO_MAX; j++) {
            unsigned char c = (unsigned char)info[i]->GetAt(j);
            line[j] = (c >= 32 && c < 127) ? c : '?';
        }
    }
    if (patch16) {
        // The frequency tables of the tuning, as the tracker driver plays them
        memcpy(mem + RMTXEX_V6_MONO_TABLES, g_Atari.GetMemoryAt(0xB000), 0x600);
    }

    // The program, the player, then the module
    bool header = true;
    for (const auto& block : blocks) {
        CAtariIO::SaveBinaryBlock(ou, mem, block.first, block.second, header);
        header = false;
    }
    CAtariIO::SaveBinaryBlock(ou, module.mem, variant.module, moduleEnd - 1, false);
    return true;
}

/// <summary>
/// Dumps, compresses and lays out the subtunes the way VU-Player V2 reads them: per subtune an entry in the song
/// index (the address of its section list and of its sequence list), the timers, and the intro and loop LZSS streams
/// from VUPlayer::SONGDATA on. Shared by the XEX and the SAP type B exports.
/// </summary>
/// <param name="subtune">The songline each subtune starts from</param>
/// <param name="mem">The Atari memory with the VU-Player loaded</param>
/// <param name="lzss_total">The first address after the last stream</param>
/// <param name="framescount">The frames of all subtunes up to their loop points</param>
bool CSongExporter::BuildLzssSubtunes(CSongExport& songExport, const int* subtune, int subsongs, byte* mem, int& lzss_total, int& framescount)
{
    int lzss_chunk = 0; // Subtune size will be added to be used as the offset to the next one
    lzss_total = 0;     // Final offset for LZSS bytes to export
    framescount = 0;

    const int frameSize = CLZSSFile::GetFrameSize(songExport.GetSong());
    int section = VUPlayer::SECTION;
    int sequence = VUPlayer::SEQUENCE;

    // LZSS buffers for each ones of the tune parts being reconstructed.
    // Because the buffers are large, they are allocated on hte heap instead of the stack.
    const size_t LZSS_BUFFER_SIZE = 0xFFFFF;
    std::vector<byte> buff2Storage(LZSS_BUFFER_SIZE);
    std::vector<byte> buff3Storage(LZSS_BUFFER_SIZE);
    byte* buff2 = buff2Storage.data();
    byte* buff3 = buff3Storage.data();

    int count = 0;
    while (count < subsongs) {
        // a LZSS export will typically make use of intro and loop only, unless specified otherwise
        int intro = 0, loop = 0;

        // LZSS buffers for each ones of the tune parts being reconstructed
        CPokeyStream pokeyStream;
        songExport.GetSong().DumpSongToPokeyStream(pokeyStream, PLAY_FROM, subtune[count], 0);

        //SetStatusBarText("Compressing data ...");

        // There is an Intro section
        if (pokeyStream.GetThirdCountPoint()) {
            intro = BruteforceOptimalLZSS(pokeyStream.GetStreamBuffer(), pokeyStream.GetThirdCountPoint() * frameSize, buff2);
        }

        // There is a Loop section
        if (pokeyStream.GetFirstCountPoint()) {
            loop = BruteforceOptimalLZSS(pokeyStream.GetStreamBuffer() + (pokeyStream.GetFirstCountPoint() * frameSize), pokeyStream.GetSecondCountPoint() * frameSize, buff3);
        }

        // Add the number of frames recorded to the total count
        framescount += pokeyStream.GetFirstCountPoint();

        // Clear the SAP-R dumper memory and reset RMT routines
        pokeyStream.FinishedRecording();

        // Some additional variables that will be used below
        int targetAddrOfModule = VUPlayer::SONGDATA + lzss_chunk; // All the LZSS data will be written starting from this address
        int lzss_startAddress = targetAddrOfModule + intro;
        int lzss_endAddress = lzss_startAddress + loop; // this sets the address that defines where the data stream has reached its end

        SetStatusBarText("");

        // If the size is too big, abort the process and show an error message
        if (lzss_endAddress > RAM_MAX_ADDRESS) {
            CString message;
            message.Format("Error, LZSS data ($%04X - $%04X) is too big to fit in memory!\n\n"
                           "The compressed data of the song takes %d bytes, the Atari has room for %d bytes ($%04X - $%04X).\n"
                           "High Instrument Speed and/or Stereo greatly inflate memory usage, even when data is compressed.\n"
                           "A shorter song, a lower instrument speed or fewer subsongs in one file make it fit.",
                           lzss_startAddress, lzss_endAddress, lzss_endAddress - VUPlayer::SONGDATA, RAM_MAX_ADDRESS + 1 - VUPlayer::SONGDATA, VUPlayer::SONGDATA, RAM_MAX_ADDRESS);
            SendErrorMessage("Buffer Overflow", message);
            return false;
        }

        // Set the song section and timer index
        int index = VUPlayer::LZSS_POINTER + count * 4;
        int timerindex = VUPlayer::SOUNGTIMER + count * 4;
        int subtunetimetotal = 0xFFFFFF / pokeyStream.GetFirstCountPoint();
        int subtunelooppoint = subtunetimetotal * pokeyStream.GetThirdCountPoint();
        int chunk = 0;

        mem[index + 0] = section & 0xFF;
        mem[index + 1] = section >> 8;
        mem[index + 2] = sequence & 0xFF;
        mem[index + 3] = sequence >> 8;
        mem[timerindex + 0] = subtunetimetotal >> 16;
        mem[timerindex + 1] = subtunetimetotal >> 8;
        mem[timerindex + 2] = subtunetimetotal & 0xFF;
        mem[timerindex + 3] = subtunelooppoint >> 16;

        // If there is an Intro section...
        if (intro) {
            memcpy(mem + targetAddrOfModule, buff2, intro);
            lzss_chunk += intro;
            mem[section + 0] = targetAddrOfModule & 0xFF;
            mem[section + 1] = targetAddrOfModule >> 8;
            mem[sequence] = chunk;
            section += 2;
            sequence += 1;
            chunk += 1;
        }

        // If there is a Loop section...
        if (loop) {
            memcpy(mem + lzss_startAddress, buff3, loop);
            lzss_chunk += loop;
            mem[section + 0] = lzss_startAddress & 0xFF;
            mem[section + 1] = lzss_startAddress >> 8;
            mem[sequence] = chunk;
            section += 2;
            sequence += 1;
            chunk += 1;
        }

        // End of data, will be overwritten if there is more data to export
        mem[section + 0] = lzss_endAddress & 0xFF;
        mem[section + 1] = lzss_endAddress >> 8;
        section += 2;
        mem[sequence] = (chunk | 0x80) - 1;
        sequence += 1;

        // Update the subtune offsets to export the next one
        lzss_total = lzss_endAddress;
        count++;
    }

    return true;
}

// The XEX dialog process was split to a different function for clarity, same will be done for SAP later...
bool CSongExporter::ShowXEXExportDialog(const CSong& song, CXEXFile& xexFile)
{
    xexFile.InitFromSong(song);

    CExpMSXDlg dlg;
    CString str;
    dlg.m_txt = DefaultXexText(xexFile);

    str.Format("Playback speed will be adjusted to %s Hz on both PAL and NTSC systems.", (xexFile.isNTSC ? "60" : "50"));
    dlg.m_speedinfo = str;

    if (dlg.DoModal() != IDOK) {
        return false;
    }

    g_rmtmsxtext = dlg.m_txt;
    g_rmtmsxtext.Replace("\x0d\x0d", "\x0d"); //13, 13 => 13

    SetXexText(xexFile, dlg.m_txt);

    xexFile.rasterbarColor = dlg.m_metercolor;
    xexFile.displayRasterbar = dlg.m_meter;
    xexFile.autoRegion = dlg.m_region_auto;

    return true;
}


CString CSongExporter::DefaultXexText(const CXEXFile& xexFile)
{
    CString EOL = "\r\n";
    if (g_rmtmsxtext != "") {
        return g_rmtmsxtext; // same from last time, making repeated exports faster
    }
    // 5 lines of text
    CString txt = CString(xexFile.songname) + EOL;
    if (xexFile.isStereo) {
        txt += "STEREO";
    }
    txt += EOL;
    txt += xexFile.currentTime.Format("%d/%m/%Y") + EOL;
    txt += "Author: (press SHIFT key)" + EOL;
    txt += "Author: ???";
    return txt;
}

void CSongExporter::SetXexText(CXEXFile& xexFile, const CString& text)
{
    // This block of code will handle all the user input text that will be inserted in the binary during the export process
    memset(xexFile.atariText, ' ', CXEXFile::ATARI_TEXT_SIZE);
    int p = 0, q = 0;
    char a;
    for (int i = 0; i < text.GetLength(); i++) {
        a = text.GetAt(i);
        if (a == '\n') {
            p += 40;
            q = 0;
        } else {
            xexFile.atariText[p + q] = a;
            q++;
        }
        if (p + q >= 5 * 40) {
            break;
        }
    }
    StrToAtariVideo((char*)xexFile.atariText, CXEXFile::ATARI_TEXT_SIZE);
}
