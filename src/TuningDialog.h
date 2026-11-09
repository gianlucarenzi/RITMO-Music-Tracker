#pragma once


#include "TuningTypes.h"

// TuningDialog dialog
class TuningDialog : public CDialog {
    // Construction
public:
    TuningDialog(CWnd* pParent = NULL); // standard constructor

    // Dialog Data
    enum { IDD = IDD_TUNING };

    TTuningSettings m_tuningSettings; // the cents of the custom temperament included

    // The temperament combo (RMT 1.36, plan 59 step 3): equal temperament first, the custom one last, the 21 presets
    // in between alphabetical by name. The combo index is no longer the temperament number: plan 59 retired seven
    // presets (5, 14, 24-28), so the entries map through TEMPERAMENT_NUMBERS. Each preset's values were checked
    // against its Scala archive source upstream, and the names corrected after it.
    static constexpr int TEMPERAMENT_COUNT = 23;
    static constexpr int TEMPERAMENT_NUMBERS[TEMPERAMENT_COUNT] = { 0,  7,  8,  21, 10, 11, 12, 13, 6, 9, 20, 17,
                                                                    18, 16, 22, 23, 15, 4,  3,  1,  2, 19, 29 };
    static constexpr const char* TEMPERAMENT_NAMES[TEMPERAMENT_COUNT] = {
        "Equal Temperament (Default)",
        "Aron-Neidhardt Equal Beating Well Temperament",
        "Atom Schisma Scale",
        "Bach Temperament by Sparschuh (A4 = 400 Hz)",
        "Bailey Modern Well Temperament (2002)",
        "Barnes Well-Tempered Clavier Temperament (1977)",
        "Béthizy Tempérament Ordinaire (1754)",
        "Big Gulp",
        "d'Alembert-Rousseau Tempérament Ordinaire (1752/1767)",
        "Equal Temperament Approximation (Minimal Order 17 Beats)",
        "Fokker 7-Limit Just Scale",
        "Pythagorean Scale",
        "Schneider Logarithmic Scale, log(4) to log(16)",
        "Upside-Down Wedding Cake (Divorce Cake)",
        "Vallotti Temperament (1754)",
        "Vallotti-Young/Werckmeister III Lesfip Scale (5-Limit, 10 Cents)",
        "Wedding Cake",
        "Werckmeister III Well Temperament (1691)",
        "Young Well Temperament (1807)",
        "Young Well Temperament No. 1 (1800)",
        "Young Well Temperament No. 2 (1799)",
        "Zarlino Tempérament Extraordinaire (Schulter)",
        "Custom Temperament in Cents",
    };
    // the combo index of a temperament number; a retired or unknown number shows as equal temperament
    static int TemperamentIndexOf(int temperament)
    {
        for (int i = 0; i < TEMPERAMENT_COUNT; i++)
            if (TEMPERAMENT_NUMBERS[i] == temperament) return i;
        return 0;
    }

protected:
    virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

    // backup values loaded during initialisation, which can then be retrieved in case the dialog was canceled
    TTuningSettings m_tuningSettingsBackup;

    // Implementation
protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnClickedIdtestnow();
    afx_msg void OnClickedIdreset();
    afx_msg void OnBnClickedCancel();
};
