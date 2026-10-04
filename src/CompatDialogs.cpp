// CompatDialogs.cpp
//
// The RMT engine is not as cleanly separated from the dialog layer as one
// might hope: a handful of engine files (Song.cpp, IO_Song.cpp,
// IO_Song_ExportAsm.cpp, IO_Importer.cpp, GUI_Song.cpp, Clipboard.cpp,
// SongExporter.cpp, ASMFileExporter.cpp) instantiate the CDialog-derived
// data classes (declared in EffectsDlg.h / filenewdlg.h / importdlgs.h /
// exportdlgs.h / ...) and call DoModal() on them. The dialogs themselves are
// Qt (qt/RmtQtDialogs.cpp), which reads and writes their data members.
//
// This file defines the out-of-line members (constructor + declared
// virtuals) of those classes, without any business logic of their own, so
// that they link in every build, RmtCoreTest included, where DoModal() is
// always cancelled (CompatTypes.h).

#include "PlatformTypes.h"

#include "SongTypes.h"
#include "Song.h"

#include "EffectsDlg.h"
#include "filenewdlg.h"
#include "importdlgs.h"
#include "exportdlgs.h"
#include "SAPFileExportDialog.h"
#include "OptionsDialog.h"
#include "Global.h" // RMT_DEFAULT_AUDIO_BUFFER_MS
#include "TuningDialog.h"

// ---------------------------------------------------------------------------
// EffectsDlg.h
// ---------------------------------------------------------------------------

// Song and track dialogs: shown by the Qt frontend (src/qt/RmtQtDialogs.cpp);
// same defaults as effectsdlg.cpp, the callers set the other members
CEffectsDlg::CEffectsDlg(CWnd* pParent) : CDialog(CEffectsDlg::IDD, pParent) {}
void CEffectsDlg::DoDataExchange(CDataExchange*) {}
BOOL CEffectsDlg::OnInitDialog() { return TRUE; }
void CEffectsDlg::OnOK() {}
void CEffectsDlg::OnCancel() {}

COctaveSelectDlg::COctaveSelectDlg(CWnd* pParent) : CDialog(COctaveSelectDlg::IDD, pParent) {}
BOOL COctaveSelectDlg::PreTranslateMessage(MSG*) { return FALSE; }
void COctaveSelectDlg::DoDataExchange(CDataExchange*) {}
void COctaveSelectDlg::OnOK() {}
BOOL COctaveSelectDlg::OnInitDialog() { return TRUE; }

CVolumeSelectDlg::CVolumeSelectDlg(CWnd* pParent) : CDialog(CVolumeSelectDlg::IDD, pParent)
{
    m_respectvolume = FALSE;
}
BOOL CVolumeSelectDlg::PreTranslateMessage(MSG*) { return FALSE; }
void CVolumeSelectDlg::DoDataExchange(CDataExchange*) {}
BOOL CVolumeSelectDlg::OnInitDialog() { return TRUE; }
void CVolumeSelectDlg::OnOK() {}

CInstrumentSelectDlg::CInstrumentSelectDlg(CWnd* pParent) : CDialog(CInstrumentSelectDlg::IDD, pParent) {}
BOOL CInstrumentSelectDlg::PreTranslateMessage(MSG*) { return FALSE; }
void CInstrumentSelectDlg::DoDataExchange(CDataExchange*) {}
BOOL CInstrumentSelectDlg::OnInitDialog() { return TRUE; }

CSongTracksOrderDlg::CSongTracksOrderDlg(CWnd* pParent) : CDialog(CSongTracksOrderDlg::IDD, pParent) {}
void CSongTracksOrderDlg::DoDataExchange(CDataExchange*) {}
BOOL CSongTracksOrderDlg::OnInitDialog() { return TRUE; }

CInstrumentChangeDlg::CInstrumentChangeDlg(CWnd* pParent) : CDialog(CInstrumentChangeDlg::IDD, pParent)
{
    m_combo1 = m_combo2 = m_combo3 = m_combo4 = m_combo5 = m_combo6 = -1;
    m_combo7 = m_combo8 = m_combo9 = m_combo10 = m_combo11 = m_combo12 = -1;
}
void CInstrumentChangeDlg::DoDataExchange(CDataExchange*) {}
BOOL CInstrumentChangeDlg::OnInitDialog() { return TRUE; }
void CInstrumentChangeDlg::OnOK() {}

CInsertCopyOrCloneOfSongLinesDlg::CInsertCopyOrCloneOfSongLinesDlg(CWnd* pParent) : CDialog(CInsertCopyOrCloneOfSongLinesDlg::IDD, pParent) {}
void CInsertCopyOrCloneOfSongLinesDlg::DoDataExchange(CDataExchange*) {}
BOOL CInsertCopyOrCloneOfSongLinesDlg::OnInitDialog() { return TRUE; }
void CInsertCopyOrCloneOfSongLinesDlg::OnOK() {}

// ---------------------------------------------------------------------------
// filenewdlg.h
// ---------------------------------------------------------------------------

// Shown by the Qt frontend (src/qt/RmtQtDialogs.cpp); same defaults as filenewdlg.cpp
CFileNewDlg::CFileNewDlg(CWnd* pParent) : CDialog(CFileNewDlg::IDD, pParent)
{
    m_maxTrackLength = 64;
    m_comboMonoOrStereo = 1; // 0 = mono 4 tracks, 1 = stereo 8 tracks
}
void CFileNewDlg::DoDataExchange(CDataExchange*) {}
void CFileNewDlg::OnOK() {}

// ---------------------------------------------------------------------------
// importdlgs.h
// ---------------------------------------------------------------------------

// Import dialogs: shown by the Qt frontend (src/qt/RmtQtDialogs.cpp); same defaults as importdlgs.cpp
CImportModDlg::CImportModDlg(CWnd* pParent) : CDialog(CImportModDlg::IDD, pParent)
{
    m_check1 = m_check2 = m_check3 = m_check4 = m_check5 = m_check6 = m_check7 = m_check8 = FALSE;
}
void CImportModDlg::DoDataExchange(CDataExchange*) {}
BOOL CImportModDlg::OnInitDialog() { return TRUE; }
void CImportModDlg::OnOK() {}

CImportModFinishedDlg::CImportModFinishedDlg(CWnd* pParent) : CDialog(CImportModFinishedDlg::IDD, pParent) {}
void CImportModFinishedDlg::DoDataExchange(CDataExchange*) {}
BOOL CImportModFinishedDlg::OnInitDialog() { return TRUE; }

CImportTmcDlg::CImportTmcDlg(CWnd* pParent) : CDialog(CImportTmcDlg::IDD, pParent)
{
    m_check1 = m_check6 = m_check7 = FALSE;
}
void CImportTmcDlg::DoDataExchange(CDataExchange*) {}
BOOL CImportTmcDlg::OnInitDialog() { return TRUE; }

CImportTmcFinishedDlg::CImportTmcFinishedDlg(CWnd* pParent) : CDialog(CImportTmcFinishedDlg::IDD, pParent) {}
void CImportTmcFinishedDlg::DoDataExchange(CDataExchange*) {}
BOOL CImportTmcFinishedDlg::OnInitDialog() { return TRUE; }

// Shown by the Qt frontend (src/qt/RmtQtDialogs.cpp)
CTracksLoadDlg::CTracksLoadDlg(CWnd* pParent) : CDialog(CTracksLoadDlg::IDD, pParent)
{
    m_radio = 0;
}
void CTracksLoadDlg::DoDataExchange(CDataExchange*) {}
void CTracksLoadDlg::OnOK() {}
BOOL CTracksLoadDlg::OnInitDialog() { return TRUE; }

// ---------------------------------------------------------------------------
// exportdlgs.h
// ---------------------------------------------------------------------------

// Export dialogs: shown by the Qt frontend (src/qt/RmtQtDialogs.cpp); same
// defaults as exportdlgs.cpp, the callers set the other members
CExportStrippedRMTDialog::CExportStrippedRMTDialog(CWnd* pParent) : CDialog(CExportStrippedRMTDialog::IDD, pParent) {}
void CExportStrippedRMTDialog::DoDataExchange(CDataExchange*) {}
BOOL CExportStrippedRMTDialog::OnInitDialog() { return TRUE; }

CExpMSXDlg::CExpMSXDlg(CWnd* pParent) : CDialog(CExpMSXDlg::IDD, pParent)
{
    m_meter = FALSE;
    m_msx_shuffle = FALSE;
    m_region_auto = FALSE;
    m_metercolor = 0;
}
void CExpMSXDlg::DoDataExchange(CDataExchange*) {}
BOOL CExpMSXDlg::OnInitDialog() { return TRUE; }
void CExpMSXDlg::OnOK() {}

CExportAsmDlg::CExportAsmDlg(CWnd* pParent) : CDialog(CExportAsmDlg::IDD, pParent)
{
    m_exportType = m_notesIndexOrFreq = m_durationsType = 0;
}
void CExportAsmDlg::DoDataExchange(CDataExchange*) {}
void CExportAsmDlg::OnOK() {}
BOOL CExportAsmDlg::OnInitDialog() { return TRUE; }

CExportRelocatableAsmForRmtPlayer::CExportRelocatableAsmForRmtPlayer(CWnd* pParent) : CDialog(CExportRelocatableAsmForRmtPlayer::IDD, pParent)
{
    m_InitPhase = FALSE;
}
void CExportRelocatableAsmForRmtPlayer::DoDataExchange(CDataExchange*) {}
BOOL CExportRelocatableAsmForRmtPlayer::OnInitDialog() { return TRUE; }

// ---------------------------------------------------------------------------
// SAPFileExportDialog.h
//
// Reached through the static Show() helper (SongExporter.cpp). The dialog is
// shown by the Qt frontend; Show() is the one of SAPFileExportDialog.cpp
// (the dialog classes also have the DDX code), kept identical.
// ---------------------------------------------------------------------------

CSAPFileExportDialog::CSAPFileExportDialog(CWnd* pParent) : CDialog(CSAPFileExportDialog::IDD, pParent) {}
void CSAPFileExportDialog::DoDataExchange(CDataExchange*) {}

bool CSAPFileExportDialog::Show(const CSong& song, CSAPFile& sapFile)
{
    CSAPFileExportDialog dlg;
    sapFile.Init(song);

    dlg.m_author = sapFile.GetAuthor();
    dlg.m_name = sapFile.GetName();
    dlg.m_date = sapFile.GetDate();

    song.GetSubsongParts(dlg.m_subsongs);

    dlg.m_title.Format("Export as SAP File of Type '%s'", (LPCTSTR)sapFile.GetType());
    if (dlg.DoModal() != IDOK) {
        return false;
    }

    sapFile.SetAuthor(dlg.m_author);
    sapFile.SetName(dlg.m_name);
    sapFile.SetDate(dlg.m_date);

    // Parses the "Subsongs" line (only the number of subsongs is used, as
    // in SAPFileExportDialog.cpp)
    CString str = dlg.m_subsongs + " "; // Add space after the last character for parsing
    str.MakeUpper();
    int subsongs = 0;
    byte n = 0, isn = 0;

    for (int i = 0; i < str.GetLength(); i++) {
        char a = str.GetAt(i);
        if (a >= '0' && a <= '9') {
            n = (n << 4) + (a - '0');
            isn = 1;
        } else if (a >= 'A' && a <= 'F') {
            n = (n << 4) + (a - 'A' + 10);
            isn = 1;
        } else {
            if (isn) {
                subsongs++;
                if (subsongs >= CSAPFile::MAXSUBSONGS) {
                    break;
                }
                isn = 0;
            }
        }
    }
    sapFile.SetSongs(subsongs);
    return true;
}

// ---------------------------------------------------------------------------
// Dialogs opened by the view (RmtView.cpp), and the message maps of all the
// stubbed dialogs (DECLARE_MESSAGE_MAP in their headers)
// ---------------------------------------------------------------------------

// Shown by the Qt frontend (src/qt/RmtQtDialogs.cpp); same defaults as OptionsDialog.cpp
COptionsDialog::COptionsDialog(CWnd* pParent) : CDialog(COptionsDialog::IDD, pParent)
{
    m_midi_TouchResponse = FALSE;
    m_midi_VolumeOffset = 0;
    m_trackLinePrimaryHighlight = 0;
    m_trackLineSecondaryHighlight = 0;
    m_scaling_percentage = 100;
    m_ntsc = FALSE;
    m_doSmoothScrolling = TRUE;
    m_displayflatnotes = FALSE;
    m_usegermannotation = FALSE;
    m_midi_NoteOff = FALSE;
    m_keyboard_updowncontinue = FALSE;
    m_nohwsoundbuffer = FALSE;
    m_audioBufferMs = RMT_DEFAULT_AUDIO_BUFFER_MS;
    m_tracklinealtnumbering = FALSE;
    m_keyboard_rememberoctavesandvolumes = FALSE;
    m_keyboard_escresetatarisound = FALSE;
    m_keyboard_askwhencontrol_s = FALSE;
    m_viewDebugDisplay = FALSE;
    m_trackerDriverVersion = NONE;
    m_midi_device = -1;
    m_keyboard_layout = KeyboardLayout::QWERTY;
}
void COptionsDialog::DoDataExchange(CDataExchange*) {}
BOOL COptionsDialog::OnInitDialog() { return TRUE; }
void COptionsDialog::OnOK() {}
void COptionsDialog::OnMidiTouchResponseClicked() {}
void COptionsDialog::OnPaths() {}

// Tuning, track length and renumber dialogs: shown by the Qt frontend
// (src/qt/RmtQtDialogs.cpp); same defaults as TuningDialog.cpp, filenewdlg.cpp
// and effectsdlg.cpp
TuningDialog::TuningDialog(CWnd* pParent) : CDialog(TuningDialog::IDD, pParent)
{
    m_tuningSettings = {};
    m_tuningRatios = {};
}
void TuningDialog::DoDataExchange(CDataExchange*) {}
BOOL TuningDialog::OnInitDialog() { return TRUE; }
void TuningDialog::OnOK() {}
void TuningDialog::OnClickedIdtestnow() {}
void TuningDialog::OnClickedIdreset() {}
void TuningDialog::OnBnClickedCancel() {}

CChangeMaxtracklenDlg::CChangeMaxtracklenDlg(CWnd* pParent) : CDialog(CChangeMaxtracklenDlg::IDD, pParent)
{
    m_maxtracklen = 0;
}
void CChangeMaxtracklenDlg::DoDataExchange(CDataExchange*) {}

CRenumberTracksDlg::CRenumberTracksDlg(CWnd* pParent) : CDialog(CRenumberTracksDlg::IDD, pParent)
{
    m_radio = 0;
}
void CRenumberTracksDlg::DoDataExchange(CDataExchange*) {}
BOOL CRenumberTracksDlg::OnInitDialog() { return TRUE; }
void CRenumberTracksDlg::OnOK() {}

CRenumberInstrumentsDlg::CRenumberInstrumentsDlg(CWnd* pParent) : CDialog(CRenumberInstrumentsDlg::IDD, pParent)
{
    m_radio = 0;
}
void CRenumberInstrumentsDlg::DoDataExchange(CDataExchange*) {}
BOOL CRenumberInstrumentsDlg::OnInitDialog() { return TRUE; }
void CRenumberInstrumentsDlg::OnOK() {}

#define RMT_EMPTY_MAP(c)          \
    BEGIN_MESSAGE_MAP(c, CDialog) \
    END_MESSAGE_MAP()
RMT_EMPTY_MAP(CEffectsDlg)
RMT_EMPTY_MAP(COctaveSelectDlg)
RMT_EMPTY_MAP(CVolumeSelectDlg)
RMT_EMPTY_MAP(CInstrumentSelectDlg)
RMT_EMPTY_MAP(CSongTracksOrderDlg)
RMT_EMPTY_MAP(CInstrumentChangeDlg)
RMT_EMPTY_MAP(CInsertCopyOrCloneOfSongLinesDlg)
RMT_EMPTY_MAP(CFileNewDlg)
RMT_EMPTY_MAP(CImportModDlg)
RMT_EMPTY_MAP(CImportModFinishedDlg)
RMT_EMPTY_MAP(CImportTmcDlg)
RMT_EMPTY_MAP(CImportTmcFinishedDlg)
RMT_EMPTY_MAP(CTracksLoadDlg)
RMT_EMPTY_MAP(CExportStrippedRMTDialog)
RMT_EMPTY_MAP(CExpMSXDlg)
RMT_EMPTY_MAP(CExportAsmDlg)
RMT_EMPTY_MAP(CExportRelocatableAsmForRmtPlayer)
RMT_EMPTY_MAP(CSAPFileExportDialog)
RMT_EMPTY_MAP(COptionsDialog)
RMT_EMPTY_MAP(TuningDialog)
RMT_EMPTY_MAP(CChangeMaxtracklenDlg)
RMT_EMPTY_MAP(CRenumberTracksDlg)
RMT_EMPTY_MAP(CRenumberInstrumentsDlg)
