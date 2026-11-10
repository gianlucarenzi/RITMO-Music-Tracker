#include "PlatformTypes.h"
#include "Song.h"
#include "Global.h"
#include "RmtModuleCodec.h"
#include "RmtModuleFile.h"
#include "AtariIO.h"
#include "exportdlgs.h"


extern AssemblerFormat g_AsmFormat;

extern WORD g_rmtstripped_adr_module; //address for export RMT stripped file
extern BOOL g_rmtstripped_sfx;        //sfx offshoot RMT stripped file
extern BOOL g_rmtstripped_gvf;        //gvs GlobalVolumeFade for feat
extern BOOL g_rmtstripped_nos;        //nos NoStartingSongline for feat


/// <summary>
/// Export the song data as assembler source code.
/// </summary>
/// <param name="ou">Output stream</param>
/// <param name="exportStrippedDesc">Data about the packed RMT module</param>
/// <param name="filename"></param>
/// <returns>true is the save went ok</returns>
bool CSong::ExportAsStrippedRMT(CSong& song, std::ofstream& ou, TExportDescription* exportStrippedDesc, LPCTSTR filename)
{
    TExportDescription exportTempDescription;
    memset(&exportTempDescription, 0, sizeof(TExportDescription));
    exportTempDescription.targetAddrOfModule = 0x4000; // Standard RMT modules are set to start @ $4000

    // Create a variant for SFX (ie. including unused instruments and tracks)
    exportTempDescription.firstByteAfterModule = song.MakeModule(exportTempDescription.mem, exportTempDescription.targetAddrOfModule, SongIOType::RMT, exportTempDescription.instrumentSavedFlags, exportTempDescription.trackSavedFlags);
    if (exportTempDescription.firstByteAfterModule < 0) return false; // if the module could not be created

    // Show the dialog to control the stripped output parameters
    CExportStrippedRMTDialog dlg;
    // Common data
    dlg.m_exportAddr = g_rmtstripped_adr_module; //global, so that it remains the same on repeated export
    dlg.m_globalVolumeFade = g_rmtstripped_gvf;
    dlg.m_noStartingSongLine = g_rmtstripped_nos;
    dlg.m_song = &song;
    dlg.m_filename = (char*)filename;
    dlg.m_sfxSupport = g_rmtstripped_sfx;
    dlg.m_assemblerFormat = g_AsmFormat;

    // Stripped RMT data
    dlg.m_moduleLengthForStrippedRMT = exportStrippedDesc->firstByteAfterModule - exportStrippedDesc->targetAddrOfModule;
    dlg.m_savedInstrFlagsForStrippedRMT = exportStrippedDesc->instrumentSavedFlags;
    dlg.m_savedTracksFlagsForStrippedRMT = exportStrippedDesc->trackSavedFlags;

    // Full RMT/SFX data
    dlg.m_moduleLengthForSFX = exportTempDescription.firstByteAfterModule - exportTempDescription.targetAddrOfModule;
    dlg.m_savedInstrFlagsForSFX = exportTempDescription.instrumentSavedFlags;
    dlg.m_savedTracksFlagsForSFX = exportTempDescription.trackSavedFlags;

    // Show the dialog and get the stripped RMT configuration parameters
    if (dlg.DoModal() != IDOK) return false;

    // Save the configurations for later reuse
    g_rmtstripped_adr_module = dlg.m_exportAddr;
    g_rmtstripped_sfx = dlg.m_sfxSupport;
    g_rmtstripped_gvf = dlg.m_globalVolumeFade;
    g_rmtstripped_nos = dlg.m_noStartingSongLine;
    g_AsmFormat = dlg.m_assemblerFormat;

    return ExportAsStrippedRMTApply(song, ou, g_rmtstripped_adr_module, g_rmtstripped_sfx);
}

/// <summary>
/// The dialog-independent half of ExportAsStrippedRMT(): regenerates the RMT module with the confirmed
/// target address and saves it - with the unused instruments and tracks stripped out when SFX support is on.
/// </summary>
bool CSong::ExportAsStrippedRMTApply(CSong& song, std::ofstream& ou, int targetAddrOfModule, BOOL sfxSupport)
{
    // SFX support keeps all non-empty instruments (sound effects trigger instruments directly, without any track
    // referencing them); without it, unused instruments are stripped. The two iotypes were swapped here since 1.34,
    // so an SFX-only module (instruments, no tracks) exported to a 30-byte husk (issue #5). Since RMT 1.36 (plan 60
    // M6) the stripped module of the song, written by CRmtModuleFile.
    try {
        CRmtModule module = CRmtModuleCodec::FromSong(song, sfxSupport ? SongIOType::RMT : SongIOType::RMTSTRIPPED);
        CRmtModule::Bytes bytes = CRmtModuleFile::Stripped(module, targetAddrOfModule).Write();
        ou.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return true; // Indicate that data was saved
    } catch (const CRmtModule::FormatException& e) {
        MessageBox(g_hwnd, CString("The song cannot be coded to the RMT module format.\n\n") + e.what(), "Internal format problem.", MB_ICONERROR);
        return false; // if the module could not be created
    }
}
