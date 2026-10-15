#if !defined(RMT_CONFIGDLG_H)
#define RMT_CONFIGDLG_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "resource.h"

#include "General.h"
#include "TrackerDriverVersion.h"

extern CString g_defaultSongsPath;       // Default path for songs
extern CString g_defaultInstrumentsPath; // Default path for instruments
extern CString g_defaultTracksPath;      // Default path for tracks

extern CString g_lastLoadPath_Songs;
extern CString g_lastLoadPath_Instruments;
extern CString g_lastLoadPath_Tracks;

/////////////////////////////////////////////////////////////////////////////
// COptionsDialog dialog

// CModulePropertiesDialog: File > Properties (Alt+Enter), the module's own settings (as in RMT 1.36, plan 44) - the
// video standard, the tracker driver and the Tuning... button - below what the module holds (shown by the Qt
// frontend, src/qt/RmtQtDialogs.cpp)
class CModulePropertiesDialog : public CDialog {
public:
    CModulePropertiesDialog(CWnd* pParent = NULL);

    enum { IDD = IDD_MODULE_PROPERTIES };
    CString m_info;                                     // the file and what the module holds, read only
    BOOL m_ntsc;                                        // NTSC (60 Hz), else PAL
    TrackerDriverVersion m_trackerDriverVersion;        // the driver of the song
    TrackerDriverVersion m_defaultTrackerDriverVersion; // the default, shown when the song plays another one
};

class COptionsDialog : public CDialog {
    // Construction
public:
    COptionsDialog(CWnd* pParent = NULL); // standard constructor

    // Dialog Data
    enum { IDD = IDD_CONFIG };
    CComboBox m_keyboard_c_layout;
    CComboBox m_midi_c_device;
    BOOL m_midi_TouchResponse;
    int m_midi_VolumeOffset;
    int m_trackLinePrimaryHighlight;
    int m_trackLineSecondaryHighlight;
    int m_scaling_percentage;
    BOOL m_doSmoothScrolling;
    BOOL m_displayflatnotes;
    BOOL m_usegermannotation;
    BOOL m_midi_NoteOff;
    BOOL m_keyboard_updowncontinue;
    BOOL m_nohwsoundbuffer;
    int m_audioBufferMs;
    BOOL m_tracklinealtnumbering;
    BOOL m_keyboard_rememberoctavesandvolumes;
    BOOL m_keyboard_escresetatarisound;
    BOOL m_keyboard_askwhencontrol_s;
    BOOL m_viewDebugDisplay;

    int m_midi_device;
    KeyboardLayout m_keyboard_layout;

    // Overrides
    // ClassWizard generated virtual function overrides
protected:
    virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

    // Implementation
protected:
    // Generated message map functions
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    afx_msg void OnMidiTouchResponseClicked();
    afx_msg void OnPaths();
    DECLARE_MESSAGE_MAP()
public:
    //afx_msg void OnBnClickedDisplayflatnotes();
};

/////////////////////////////////////////////////////////////////////////////
// COptionsPathsDlg dialog

class COptionsPathsDlg : public CDialog {
    // Construction
public:
    COptionsPathsDlg(CWnd* pParent = NULL); // standard constructor

    void BrowsePath(int itemID);

    // Dialog Data
    enum { IDD = IDD_PATHS };
    CString m_path_songs;
    CString m_path_instruments;
    CString m_path_tracks;


    // Overrides
    // ClassWizard generated virtual function overrides
protected:
    virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

    // Implementation
protected:
    // Generated message map functions
    afx_msg void OnButton1();
    afx_msg void OnButton2();
    afx_msg void OnButton3();
    DECLARE_MESSAGE_MAP()
};

#endif // !defined(RMT_CONFIGDLG_H)
