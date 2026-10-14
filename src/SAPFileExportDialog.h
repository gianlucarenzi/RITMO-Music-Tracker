#pragma once

#include "resource.h"

#include "SAPFile.h"

class CSong;


/////////////////////////////////////////////////////////////////////////////
// CSAPFileExportDialog dialog

class CSAPFileExportDialog : public CDialog {
    // Construction
public:
    CSAPFileExportDialog(CWnd* pParent = NULL); // standard constructor

    static bool Show(const CSong& song, CSAPFile& sapFile);

    // Dialog Data
    CString m_title;
    enum { IDD = IDD_EXPSAP };
    CString m_author;
    CString m_date;
    CString m_name;
    CString m_subsongs;
    BOOL m_noSubsongs = FALSE; // the XEX export with the RMT player: NAME, AUTHOR and DATE only


    // Overrides
    // ClassWizard generated virtual function overrides
protected:
    virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

    // Implementation
protected:
    // Generated message map functions
    // NOTE: the ClassWizard will add member functions here
    DECLARE_MESSAGE_MAP()

    virtual BOOL OnInitDialog() override
    {
        CDialog::OnInitDialog();
        SetWindowText(m_title);
        return TRUE;
    }
};
