// RmtQtDialogs.cpp - the dialogs of RITMO, in Qt (see RmtQtDialogs.h)

#include "RmtQtDialogs.h"

#include "resource.h"
#include "Global.h"
#include "TrackTypes.h"
#include "filenewdlg.h"
#include "importdlgs.h"
#include "Song.h"
#include "exportdlgs.h"
#include "SAPFileExportDialog.h"
#include "OptionsDialog.h"
#include "ASMFileExporter.h"
#include "Notes.h"
#include "EffectsDlg.h"
#include "TuningDialog.h"
#include "Tuning.h"
#include "Tracks.h"
#include "Instruments.h"
#include "IOHelpers.h"
#include "GuiHelpers.h"

#include <utility>

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>

extern CSong g_Song;
extern CInstruments g_Instruments;
extern CTuning g_Tuning;

// Run a dialog. Test runs (RMT_QT_GRAB) show no modal dialogs: they are
// cancelled, unless RMT_QT_DIALOG=file.png is set, then the dialog is shown,
// saved and confirmed with ok(), what the user does to confirm it. The first
// dialog is saved to file.png, the next ones to file-2.png, file-3.png...
static bool ExecDialog(QDialog& dialog, const std::function<void()>& ok)
{
    if (!qEnvironmentVariableIsEmpty("RMT_QT_GRAB")) {
        QString shot = qEnvironmentVariable("RMT_QT_DIALOG");
        if (shot.isEmpty()) {
            qWarning("[Dialog] %s: cancelled", qPrintable(dialog.windowTitle()));
            return false;
        }
        static int count = 0;
        if (++count > 1) {
            QFileInfo fi(shot);
            shot = fi.path() + "/" + fi.completeBaseName() + QString("-%1.").arg(count) + fi.suffix();
        }
        qWarning("[Dialog] %s: %s", qPrintable(dialog.windowTitle()), qPrintable(shot));
        QTimer::singleShot(500, &dialog, [&dialog, shot, ok] {
            dialog.grab().save(shot);
            ok();
        });
    }
    return dialog.exec() == QDialog::Accepted;
}

// ---------------------------------------------------------------------------
// IDD_FILENEW - File -> New (CFileNewDlg, filenewdlg.cpp)
// ---------------------------------------------------------------------------

static INT_PTR RunFileNew(QWidget* parent, CFileNewDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("New RMT module");

    auto* length = new QSpinBox(&dialog);
    length->setRange(1, TRACKLEN); // DDV_MinMaxInt(1, 256)
    length->setValue(dlg->m_maxTrackLength);

    auto* type = new QComboBox(&dialog);
    type->addItems({ "MONO - 4 TRACKS", "STEREO - 8 TRACKS" });
    type->setCurrentIndex(dlg->m_comboMonoOrStereo);

    auto* form = new QFormLayout;
    form->addRow("Maximal length of tracks", length);
    form->addRow(type);

    auto* warning = new QLabel("Warning: Current data will be discarded!\nThis operation cannot be undone!", &dialog);
    warning->setAlignment(Qt::AlignCenter);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(warning);
    layout->addWidget(buttons);

    // CFileNewDlg::OnOK(): tracks longer than 64 lines need a confirmation
    auto ok = [&] {
        if (length->value() > 64) {
            int r = MessageBox(g_hwnd, "Warning:\nLength of tracks is greater than 64.\nRMT's internal module format allows for a maximum of\n256 bytes for each track. It is not recommended to use\na large number of events in long tracks.\nEach track event (note or speed command) uses about 2 bytes.\n\nWhen saving the RMT file it will report any problems with it.\n\nOk?", "New RMT module - Warning", MB_YESNO | MB_ICONQUESTION);
            if (r != IDYES) return;
        }
        dialog.accept();
    };
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, ok);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (!ExecDialog(dialog, ok)) return IDCANCEL;
    dlg->m_maxTrackLength = length->value();
    dlg->m_comboMonoOrStereo = type->currentIndex();
    return IDOK;
}

// ---------------------------------------------------------------------------
// IDD_IMPORTMOD / IDD_IMPORTTMC - import options (CImportModDlg,
// CImportTmcDlg, importdlgs.cpp)
// ---------------------------------------------------------------------------

static QCheckBox* AddCheck(QVBoxLayout* layout, const char* text, int indent = 0)
{
    auto* check = new QCheckBox(text);
    check->setChecked(true); // OnInitDialog: all options on
    if (indent) {
        auto* row = new QHBoxLayout;
        row->addSpacing(indent);
        row->addWidget(check);
        layout->addLayout(row);
    } else
        layout->addWidget(check);
    return check;
}

static QLabel* AddHeading(QVBoxLayout* layout, const char* text)
{
    auto* label = new QLabel(text);
    layout->addSpacing(4);
    layout->addWidget(label);
    return label;
}

static QDialogButtonBox* AddOkCancel(QDialog& dialog, QVBoxLayout* layout)
{
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    return buttons;
}

static INT_PTR RunImportMod(QWidget* parent, CImportModDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Import ProTracker Module");
    auto* layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel(QString::fromLocal8Bit(dlg->m_info.GetString())));

    auto* typeBox = new QGroupBox("Type of RMT module");
    auto* typeLayout = new QVBoxLayout(typeBox);
    auto* radio1 = new QRadioButton(QString::fromLocal8Bit(dlg->m_txtradio1.GetString()));
    auto* radio2 = new QRadioButton(QString::fromLocal8Bit(dlg->m_txtradio2.GetString()));
    radio1->setChecked(true);
    typeLayout->addWidget(radio1);
    typeLayout->addWidget(radio2);
    layout->addWidget(typeBox);

    AddHeading(layout, "Note events:");
    auto* check1 = AddCheck(layout, "Shift down octave of all instruments if song tuning is too high and if it is possible.");
    auto* check5 = AddCheck(layout, "Substitute all portamento effects by inserting of calculated notes.");
    AddHeading(layout, "Volume events:");
    auto* check2 = AddCheck(layout, "Increase the volume entries in tracks to spread full volume range.");
    auto* check3 = AddCheck(layout, "Decrease the instruments' volume envelopes in accordance with tracks entries increasing.", 16);
    auto* check4 = AddCheck(layout, "Decrease the instruments' volume envelopes according to sample volume entry.");
    AddHeading(layout, "Special:");
    auto* check8 = AddCheck(layout, "Use the Fourier transformation for detection of samples' tunings.");
    check8->setChecked(false); // WS_DISABLED in the resource
    check8->setEnabled(false);
    AddHeading(layout, "Size optimizations:");
    auto* check6 = AddCheck(layout, "Search and build wise loops in tracks.");
    auto* check7 = AddCheck(layout, "Truncate unused parts of tracks (only if it has data saving effect).");

    // CImportModDlg::OnCheck2(): check 3 only applies with check 2
    QObject::connect(check2, &QCheckBox::toggled, check3, [check3](bool on) {
        check3->setEnabled(on);
        check3->setChecked(on);
    });

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { buttons->button(QDialogButtonBox::Ok)->click(); })) return IDCANCEL;

    // CImportModDlg::OnOK(): the text of the radio button not chosen is cleared
    if (radio2->isChecked())
        dlg->m_txtradio1 = "";
    else
        dlg->m_txtradio2 = "";
    dlg->m_check1 = check1->isChecked();
    dlg->m_check2 = check2->isChecked();
    dlg->m_check3 = check3->isChecked();
    dlg->m_check4 = check4->isChecked();
    dlg->m_check5 = check5->isChecked();
    dlg->m_check6 = check6->isChecked();
    dlg->m_check7 = check7->isChecked();
    dlg->m_check8 = check8->isChecked();
    return IDOK;
}

static INT_PTR RunImportTmc(QWidget* parent, CImportTmcDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Import Theta Music Composer module");
    auto* layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel(QString::fromLocal8Bit(dlg->m_info.GetString())));
    AddHeading(layout, "Instruments:");
    auto* check1 = AddCheck(layout, "Permit to use the instrument table also for vibrato and some special TMC effects.");
    AddHeading(layout, "Size optimizations:");
    auto* check6 = AddCheck(layout, "Search and build wise loops in tracks.");
    auto* check7 = AddCheck(layout, "Truncate unused parts of tracks (only if it has data saving effect).");

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { buttons->button(QDialogButtonBox::Ok)->click(); })) return IDCANCEL;

    dlg->m_check1 = check1->isChecked();
    dlg->m_check6 = check6->isChecked();
    dlg->m_check7 = check7->isChecked();
    return IDOK;
}

// ---------------------------------------------------------------------------
// IDD_IMPORTMODFINISHED / IDD_IMPORTTMCFINISHED - the import result
// (CImportModFinishedDlg, CImportTmcFinishedDlg): OK only once "I understand"
// is checked, which is remembered for the next import of the same kind
// ---------------------------------------------------------------------------

static INT_PTR RunImportFinished(QWidget* parent, const char* title, const CString& info, const char* warning, bool& understood)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* layout = new QVBoxLayout(&dialog);

    auto* finished = new QLabel("Import of module finished.");
    finished->setAlignment(Qt::AlignCenter);
    layout->addWidget(finished);

    QString text = QString::fromLocal8Bit(info.GetString());
    text.replace("\r\n", "\n");
    auto* infoLabel = new QLabel(text);
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(infoLabel);

    // A top-level layout does not ask a word-wrapped label for its height at
    // the final width, so give it a width and the height that goes with it
    auto* warningLabel = new QLabel(warning);
    warningLabel->setAlignment(Qt::AlignCenter);
    warningLabel->setWordWrap(true);
    warningLabel->setFixedWidth(360);
    warningLabel->setMinimumHeight(warningLabel->heightForWidth(360));
    layout->addSpacing(6);
    layout->addWidget(warningLabel, 0, Qt::AlignHCenter);

    auto* row = new QHBoxLayout;
    auto* icon = new QLabel;
    icon->setPixmap(dialog.style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(32, 32));
    auto* check = new QCheckBox("Yes... Ok, ok... I understand.");
    check->setChecked(understood);
    row->addWidget(icon);
    row->addWidget(check);
    row->addStretch();
    layout->addLayout(row);

    auto* buttons = AddOkCancel(dialog, layout);
    QPushButton* okButton = buttons->button(QDialogButtonBox::Ok);
    okButton->setEnabled(understood);
    QObject::connect(check, &QCheckBox::toggled, okButton, [&understood, okButton](bool on) {
        understood = on;
        okButton->setEnabled(on);
    });

    auto ok = [check, okButton] { check->setChecked(true); okButton->click(); };
    return ExecDialog(dialog, ok) ? IDOK : IDCANCEL;
}

static bool g_importModUnderstood = false; // g_importmodyesokok of importdlgs.cpp
static bool g_importTmcUnderstood = false; // g_importtmcyesokok


// ---------------------------------------------------------------------------
// Export options (exportdlgs.cpp, SAPFileExportDialog.cpp)
// ---------------------------------------------------------------------------

static QString FromCString(const CString& s) { return QString::fromLocal8Bit(s.GetString()); }
static CString ToCString(const QString& s) { return CString(s.toLocal8Bit().constData()); }

// A line edit that turns what is typed to upper case (ES_UPPERCASE)
static QLineEdit* UpperCaseEdit(const QString& text)
{
    auto* edit = new QLineEdit(text.toUpper());
    QObject::connect(edit, &QLineEdit::textEdited, edit, [edit](const QString& t) {
        if (t != t.toUpper()) {
            int pos = edit->cursorPosition();
            edit->setText(t.toUpper());
            edit->setCursorPosition(pos);
        }
    });
    return edit;
}

// A fixed pitch font; the system one may not be (e.g. QT_QPA_PLATFORM=offscreen)
static QFont MonoFont()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (!QFontInfo(font).fixedPitch()) {
        font.setFamily("Monospace");
        font.setStyleHint(QFont::TypeWriter);
    }
    return font;
}

// A read-only text box for generated assembler text
static QPlainTextEdit* AsmTextBox()
{
    auto* text = new QPlainTextEdit;
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFont(MonoFont());
    text->setTabStopDistance(QFontMetricsF(text->font()).horizontalAdvance(' ') * 8);
    text->setMinimumSize(560, 200);
    return text;
}

static QComboBox* AsmFormatCombo(AssemblerFormat& format)
{
    auto* combo = new QComboBox;
    combo->addItems({ "Atasm", "Xasm" }); // index = AssemblerFormat
    if (format < ATASM) format = ATASM;
    if (format > XASM) format = XASM;
    combo->setCurrentIndex(format);
    return combo;
}

static void ClickOk(QDialogButtonBox* buttons) { buttons->button(QDialogButtonBox::Ok)->click(); }

// IDD_EXPORT_STRIPPED_RMT - CExportStrippedRMTDialog
static INT_PTR RunExportStrippedRmt(QWidget* parent, CExportStrippedRMTDialog* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Export RMT stripped file");
    auto* layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel("Memory location"));
    auto* address = new QLineEdit(QString::asprintf("%04X", dlg->m_exportAddr));
    address->setMaxLength(4);
    address->setMaximumWidth(60);
    address->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{0,4}"), address));
    QObject::connect(address, &QLineEdit::textEdited, address, [address](const QString& t) {
        int pos = address->cursorPosition();
        address->setText(t.toUpper());
        address->setCursorPosition(pos);
    });
    auto* info = new QLabel;
    auto* addressRow = new QHBoxLayout;
    addressRow->addWidget(new QLabel("From address (HEX):"));
    addressRow->addWidget(address);
    addressRow->addWidget(info, 1, Qt::AlignCenter);
    layout->addLayout(addressRow);

    auto* format = AsmFormatCombo(dlg->m_assemblerFormat);
    auto* formatRow = new QHBoxLayout;
    formatRow->addWidget(new QLabel("Assembler format:"));
    formatRow->addWidget(format);
    formatRow->addStretch();
    layout->addLayout(formatRow);

    auto* sfx = new QCheckBox("SFX support (also preserve unused tracks and instruments in module)");
    auto* gvf = new QCheckBox("GlobalVolumeFade support (RMTGLOBALVOLUMEFADE variable)");
    auto* nos = new QCheckBox("No songline start (always start from songline 0)");
    sfx->setChecked(dlg->m_sfxSupport);
    gvf->setChecked(dlg->m_globalVolumeFade);
    nos->setChecked(dlg->m_noStartingSongLine);
    layout->addWidget(sfx);
    layout->addWidget(gvf);
    layout->addWidget(nos);

    layout->addWidget(new QLabel("RMT FEATures definitions (for optimizations of RMT player assembler routine)"));
    auto* rmtfeat = AsmTextBox();
    layout->addWidget(rmtfeat);
    auto* copy = new QPushButton("Copy all to clipboard");
    QObject::connect(copy, &QPushButton::clicked, rmtfeat, [rmtfeat] {
        QGuiApplication::clipboard()->setText(rmtfeat->toPlainText());
    });
    layout->addWidget(copy, 0, Qt::AlignRight);

    auto* warning = new QLabel;
    warning->setWordWrap(true);
    layout->addWidget(warning);

    // CExportStrippedRMTDialog::ChangeParams(): the address is kept inside
    // the 64 KB, the RMTFEAT definitions follow the options
    auto changeParams = [&] {
        int adr = (int)strtoul(address->text().toLatin1().constData(), nullptr, 16);
        dlg->m_assemblerFormat = (AssemblerFormat)format->currentIndex();
        dlg->m_sfxSupport = sfx->isChecked();
        int length = dlg->m_sfxSupport ? dlg->m_moduleLengthForSFX : dlg->m_moduleLengthForStrippedRMT;
        if (adr > 0x10000 - length) adr = 0x10000 - length;
        dlg->m_exportAddr = adr;
        info->setText(QString::asprintf("=>  $%04X - $%04X , length $%04X (%u bytes)", adr, adr + length - 1, length, length));
        warning->setText(dlg->m_sfxSupport
                             ? "Warning:\nThis output file doesn't contain song name and names of all instruments."
                             : "Warning:\nThis output file doesn't contain any unused or empty tracks and instruments, song name and names of all instruments.");
        dlg->m_globalVolumeFade = gvf->isChecked();
        dlg->m_noStartingSongLine = nos->isChecked();

        BYTE* instrsav = dlg->m_sfxSupport ? dlg->m_savedInstrFlagsForSFX : dlg->m_savedInstrFlagsForStrippedRMT;
        BYTE* tracksav = dlg->m_sfxSupport ? dlg->m_savedTracksFlagsForSFX : dlg->m_savedTracksFlagsForStrippedRMT;
        CString s;
        CASMFileExporter::ComposeRMTFEATstring(*dlg->m_song, s, dlg->m_filename, instrsav, tracksav,
                                               dlg->m_sfxSupport, dlg->m_globalVolumeFade, dlg->m_noStartingSongLine, dlg->m_assemblerFormat);
        rmtfeat->setPlainText(FromCString(s).replace("\r\n", "\n"));
    };
    QObject::connect(address, &QLineEdit::textChanged, &dialog, changeParams);
    QObject::connect(format, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, changeParams);
    QObject::connect(sfx, &QCheckBox::toggled, &dialog, changeParams);
    QObject::connect(gvf, &QCheckBox::toggled, &dialog, changeParams);
    QObject::connect(nos, &QCheckBox::toggled, &dialog, changeParams);
    changeParams();

    auto* buttons = AddOkCancel(dialog, layout);
    return ExecDialog(dialog, [buttons] { ClickOk(buttons); }) ? IDOK : IDCANCEL;
}

// IDD_EXPMSX - CExpMSXDlg (XEX with the LZSS driver); the options are kept
// for the next export like g_msxcheck, g_msx_shuffle, g_region_auto, g_msxcol
// (also set by the script command "export xex", ScriptRunner.cpp)
BOOL g_msxcheck = 1;
BOOL g_msx_shuffle = 1;
BOOL g_region_auto = 1;
int g_msxcol = 6;

static INT_PTR RunExportXex(QWidget* parent, CExpMSXDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Export Atari executable MSX");
    auto* layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel("Text displayed on screen during music playback. 4+1 lines, 40 characters per line.\n"
                                 "The 5th line of text is shown instead of 4th line when the Shift key is held down."));
    auto* edit = new QPlainTextEdit(FromCString(dlg->m_txt).remove('\r'));
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setFont(MonoFont());
    edit->setMinimumHeight(110);
    layout->addWidget(edit);

    auto* previewLabel = new QLabel("MSX screen preview");
    previewLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(previewLabel);
    auto* preview = new QPlainTextEdit;
    preview->setReadOnly(true);
    preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    preview->setFont(MonoFont());
    preview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    preview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QFontMetrics fm(preview->font());
    preview->setFixedSize(fm.horizontalAdvance(QString(40, 'W')) + 16, fm.lineSpacing() * 4 + 16);
    layout->addWidget(preview, 0, Qt::AlignHCenter);
    auto* shiftTest = new QPushButton("Atari SHIFTkey test");
    shiftTest->setCheckable(true);
    layout->addWidget(shiftTest, 0, Qt::AlignHCenter);

    auto* rasterbar = new QCheckBox("Display rasterbar for CPU usage");
    auto* shuffle = new QCheckBox("Shuffle the rasterbar colors");
    rasterbar->setChecked(g_msxcheck != 0);
    shuffle->setChecked(g_msx_shuffle != 0);
    auto* checkRow = new QHBoxLayout;
    checkRow->addWidget(rasterbar);
    checkRow->addWidget(shuffle);
    layout->addLayout(checkRow);

    auto* color = new QScrollBar(Qt::Horizontal);
    color->setRange(1, 127);
    color->setPageStep(8);
    color->setValue(g_msxcol / 2);
    auto* colorInfo = new QLabel;
    colorInfo->setMinimumWidth(160);
    auto* colorRow = new QHBoxLayout;
    colorRow->addWidget(new QLabel("Color:"));
    colorRow->addWidget(color, 1);
    colorRow->addWidget(colorInfo);
    layout->addLayout(colorRow);

    layout->addWidget(new QLabel(FromCString(dlg->m_speedinfo)));
    auto* regionAuto = new QCheckBox("Automatically adjust playback speed");
    regionAuto->setChecked(g_region_auto != 0);
    layout->addWidget(regionAuto);

    // CExpMSXDlg::OnHScroll(): the color is 2 x the scroll position
    auto changeColor = [&](int c) {
        static const char* bar[] = { "Gray", "Rust", "Orange", "Red-orange", "Pink", "Purple", "Cobalt blue", "Blue",
                                     "Medium blue", "Dark blue", "Blue-grey", "Olive green", "Medium green", "Dark green", "Orange-green", "Brown" };
        g_msxcol = c * 2;
        colorInfo->setText(QString::asprintf("%i = %s %i", g_msxcol, bar[g_msxcol / 16], g_msxcol % 16));
    };
    QObject::connect(color, &QScrollBar::valueChanged, &dialog, changeColor);
    changeColor(color->value());

    // CExpMSXDlg::ChangeParams(): 5 lines of up to 40 characters; the preview
    // shows lines 1-4, or 1-3 and 5 while the SHIFT key test is on
    auto changeParams = [&] {
        QString s = edit->toPlainText(), d, d4th, d5th;
        int from = 0, line = 0;
        while (from < s.length() && line < 5) {
            int i = s.indexOf('\n', from);
            if (i >= 0) {
                QString l = s.mid(from, std::min(i - from, 40)) + "\r\n";
                d += l;
                if (line != 4) d4th += l;
                if (line != 3) d5th += l;
                from = i + 1;
                line++;
            } else {
                QString l = s.mid(from, 40);
                d += l;
                if (line != 4) d4th += l;
                if (line != 3) d5th += l;
                break;
            }
        }
        preview->setPlainText((shiftTest->isChecked() ? d5th : d4th).remove('\r'));
        dlg->m_txt = ToCString(d);
        bool meter = rasterbar->isChecked();
        color->setEnabled(meter);
        colorInfo->setEnabled(meter);
        shuffle->setEnabled(meter);
    };
    QObject::connect(edit, &QPlainTextEdit::textChanged, &dialog, changeParams);
    QObject::connect(shiftTest, &QPushButton::toggled, &dialog, changeParams);
    QObject::connect(rasterbar, &QCheckBox::toggled, &dialog, changeParams);
    changeParams();

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;

    // CExpMSXDlg::OnOK() and its DDX
    dlg->m_metercolor = g_msxcol;
    g_msx_shuffle = shuffle->isChecked();
    g_region_auto = regionAuto->isChecked();
    g_msxcheck = rasterbar->isChecked();
    dlg->m_meter = rasterbar->isChecked();
    dlg->m_msx_shuffle = shuffle->isChecked();
    dlg->m_region_auto = regionAuto->isChecked();
    return IDOK;
}

// IDD_EXPORT_ASM - CExportAsmDlg (ASM simple notation)
static INT_PTR RunExportAsm(QWidget* parent, CExportAsmDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Export ASM simple notation source file");
    auto* layout = new QVBoxLayout(&dialog);

    // Three groups of radio buttons; the first of each is the default
    auto addGroup = [&](const char* title, std::initializer_list<QString> texts) {
        layout->addWidget(new QLabel(title));
        auto* group = new QButtonGroup(&dialog);
        int id = 1;
        for (const QString& text : texts) {
            auto* radio = new QRadioButton(text);
            group->addButton(radio, id++);
            layout->addWidget(radio);
        }
        group->button(1)->setChecked(true);
        return group;
    };
    auto* exportType = addGroup("Export type", { "Tracks", "Whole song by song columns" });
    auto* noteValues = addGroup("Note values", { QString::asprintf("Note indexes $00-$%02X", CNotes::NOTESNUM - 1),
                                                 "Note frequencies according to distortion in first envelope column" });
    auto* durations = addGroup("Note durations", { "Notes only (special value XXX in empty beats)",
                                                   "Pairs of note,duration", "Pairs of duration,note" });

    layout->addWidget(new QLabel("Generate labels with prefix (empty prefix => no labels)"));
    auto* prefix = new QLineEdit(FromCString(dlg->m_prefixForAllAsmLabels));
    prefix->setMaxLength(32); // DDV_MaxChars(32)
    prefix->setMaximumWidth(200);
    layout->addWidget(prefix);

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;

    // CExportAsmDlg::OnOK(): 1-based index of the chosen button of each group
    dlg->m_exportType = exportType->checkedId();
    dlg->m_notesIndexOrFreq = noteValues->checkedId();
    dlg->m_durationsType = durations->checkedId();
    dlg->m_prefixForAllAsmLabels = ToCString(prefix->text());
    return IDOK;
}

// IDD_EXPORT_RMTPLAYER_ASM - CExportRelocatableAsmForRmtPlayer
static INT_PTR RunExportRelocatableAsm(QWidget* parent, CExportRelocatableAsmForRmtPlayer* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Export ASM for RmtPlayer.asm");
    auto* layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel("The RMT song will be exported as byte definitions with specific labels.\n"
                                 "The code is fully relocatable and can be split over various locations in memory!"));

    // CExportRelocatableAsmForRmtPlayer::OnInitDialog(): default labels
    if (dlg->m_strAsmLabelForStartOfSong.IsEmpty()) dlg->m_strAsmLabelForStartOfSong = "RMT_SONG_DATA";
    if (dlg->m_strAsmTracksLabel.IsEmpty()) dlg->m_strAsmTracksLabel = "RMT_SONG_TRACKS";
    if (dlg->m_strAsmSongLinesLabel.IsEmpty()) dlg->m_strAsmSongLinesLabel = "RMT_SONG_LINES";
    if (dlg->m_strAsmInstrumentsLabel.IsEmpty()) dlg->m_strAsmInstrumentsLabel = "RMT_INSTRUMENT_DATA";

    auto* grid = new QGridLayout;
    auto* songLabel = UpperCaseEdit(FromCString(dlg->m_strAsmLabelForStartOfSong));
    grid->addWidget(new QLabel("ASM Label for start of song data:"), 0, 0, 1, 2);
    grid->addWidget(songLabel, 0, 2);
    auto addRelocation = [&](int row, const char* check, const char* label, const CString& value, BOOL on) {
        auto* box = new QCheckBox(check);
        box->setChecked(on);
        auto* edit = UpperCaseEdit(FromCString(value));
        auto* text = new QLabel(label);
        text->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(box, row, 0);
        grid->addWidget(text, row, 1);
        grid->addWidget(edit, row, 2);
        return std::make_pair(box, edit);
    };
    auto [instrCheck, instrEdit] = addRelocation(1, "Relocate instruments to", "ASM Instruments Label:", dlg->m_strAsmInstrumentsLabel, dlg->m_wantRelocatableInstruments);
    auto [tracksCheck, tracksEdit] = addRelocation(2, "Relocate tracks to", "ASM Tracks Label:", dlg->m_strAsmTracksLabel, dlg->m_wantRelocatableTracks);
    auto [songCheck, songEdit] = addRelocation(3, "Relocatable song lines to", "ASM Song Label:", dlg->m_strAsmSongLinesLabel, dlg->m_wantRelocatableSongLines);
    layout->addLayout(grid);

    auto* format = AsmFormatCombo(dlg->m_assemblerFormat);
    auto* formatRow = new QHBoxLayout;
    formatRow->addWidget(new QLabel("Assembler format:"));
    formatRow->addWidget(format);
    formatRow->addStretch();
    layout->addLayout(formatRow);

    auto* sfx = new QCheckBox("SFX support (also preserve unused tracks and instruments in module)");
    auto* gvf = new QCheckBox("GlobalVolumeFade support (RMTGLOBALVOLUMEFADE variable)");
    auto* nos = new QCheckBox("No songline start (always start from songline 0)");
    sfx->setChecked(dlg->m_sfxSupport);
    gvf->setChecked(dlg->m_globalVolumeFade);
    nos->setChecked(dlg->m_noStartingSongLine);
    layout->addWidget(sfx);
    layout->addWidget(gvf);
    layout->addWidget(nos);

    auto* info = new QLabel;
    info->setAlignment(Qt::AlignCenter);
    layout->addWidget(info);
    auto* sizes = AsmTextBox();
    layout->addWidget(sizes);

    // CExportRelocatableAsmForRmtPlayer::ChangeParams(): label edits follow
    // their check box, the text shows the size of each part
    auto changeParams = [&] {
        dlg->m_wantRelocatableTracks = tracksCheck->isChecked();
        dlg->m_wantRelocatableSongLines = songCheck->isChecked();
        dlg->m_wantRelocatableInstruments = instrCheck->isChecked();
        tracksEdit->setEnabled(dlg->m_wantRelocatableTracks);
        songEdit->setEnabled(dlg->m_wantRelocatableSongLines);
        instrEdit->setEnabled(dlg->m_wantRelocatableInstruments);
        dlg->m_strAsmLabelForStartOfSong = ToCString(songLabel->text());
        dlg->m_strAsmTracksLabel = ToCString(tracksEdit->text());
        dlg->m_strAsmSongLinesLabel = ToCString(songEdit->text());
        dlg->m_strAsmInstrumentsLabel = ToCString(instrEdit->text());

        dlg->m_sfxSupport = sfx->isChecked();
        TExportDescription* desc = dlg->m_sfxSupport ? dlg->m_exportDescWithSFX : dlg->m_exportDescStripped;
        int len = desc->firstByteAfterModule - desc->targetAddrOfModule;
        info->setText(QString::asprintf("Length $%04X (%u bytes)", len, len));

        dlg->m_assemblerFormat = (AssemblerFormat)format->currentIndex();
        dlg->m_globalVolumeFade = gvf->isChecked();
        dlg->m_noStartingSongLine = nos->isChecked();

        CString s;
        CASMFileExporter::BuildRelocatableAsm(*dlg->m_song, s, desc, "",
                                              dlg->m_wantRelocatableTracks ? dlg->m_strAsmTracksLabel : CString(""),
                                              dlg->m_wantRelocatableSongLines ? dlg->m_strAsmSongLinesLabel : CString(""),
                                              dlg->m_wantRelocatableInstruments ? dlg->m_strAsmInstrumentsLabel : CString(""),
                                              dlg->m_assemblerFormat, dlg->m_sfxSupport, false, false,
                                              true); // just the size info
        sizes->setPlainText(FromCString(s).replace("\r\n", "\n"));
    };
    for (QLineEdit* edit : { songLabel, instrEdit, tracksEdit, songEdit })
        QObject::connect(edit, &QLineEdit::textChanged, &dialog, changeParams);
    for (QCheckBox* box : { instrCheck, tracksCheck, songCheck, sfx, gvf, nos })
        QObject::connect(box, &QCheckBox::toggled, &dialog, changeParams);
    QObject::connect(format, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, changeParams);
    changeParams();

    auto* buttons = AddOkCancel(dialog, layout);
    return ExecDialog(dialog, [buttons] { ClickOk(buttons); }) ? IDOK : IDCANCEL;
}

// IDD_EXPSAP - CSAPFileExportDialog (SAP-R, SAP with the LZSS driver)
static INT_PTR RunExportSap(QWidget* parent, CSAPFileExportDialog* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(FromCString(dlg->m_title));
    dialog.setMinimumWidth(560);
    auto* layout = new QVBoxLayout(&dialog);

    auto addField = [&](const char* label, const CString& value) {
        auto* text = new QLabel(label);
        text->setWordWrap(true);
        layout->addWidget(text);
        auto* edit = new QLineEdit(FromCString(value));
        layout->addWidget(edit);
        return edit;
    };
    auto* name = addField("NAME", dlg->m_name);
    auto* author = addField("AUTHOR", dlg->m_author);
    auto* date = addField("DATE", dlg->m_date);
    date->setMaximumWidth(120);
    auto* subsongs = addField("Subsongs (hexadecimal song line number for each subsong, first is default song):", dlg->m_subsongs);

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;

    dlg->m_name = ToCString(name->text());
    dlg->m_author = ToCString(author->text());
    dlg->m_date = ToCString(date->text());
    dlg->m_subsongs = ToCString(subsongs->text());
    return IDOK;
}

// ---------------------------------------------------------------------------
// IDD_CONFIG - View -> Configuration (COptionsDialog, OptionsDialog.cpp)
// ---------------------------------------------------------------------------

// IDD_PATHS - COptionsPathsDlg: default folders of songs, instruments and
// tracks; like COptionsDialog::OnPaths() they apply at once on OK
static void RunConfigPaths(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Paths...");
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);

    auto addPath = [&](const char* label, const CString& value) {
        layout->addWidget(new QLabel(label));
        auto* edit = new QLineEdit(FromCString(value));
        auto* browse = new QPushButton("Browse");
        QObject::connect(browse, &QPushButton::clicked, &dialog, [&dialog, edit] { // CFilePathDlg
            QString dir = QFileDialog::getExistingDirectory(&dialog, "Select folder", edit->text());
            if (!dir.isEmpty()) edit->setText(dir);
        });
        auto* row = new QHBoxLayout;
        row->addWidget(edit, 1);
        row->addWidget(browse);
        layout->addLayout(row);
        return edit;
    };
    auto* songs = addPath("Songs:", g_defaultSongsPath);
    auto* instruments = addPath("Instruments:", g_defaultInstrumentsPath);
    auto* tracks = addPath("Tracks:", g_defaultTracksPath);

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return;

    g_defaultSongsPath = ToCString(songs->text());
    g_defaultInstrumentsPath = ToCString(instruments->text());
    g_defaultTracksPath = ToCString(tracks->text());
    g_lastLoadPath_Songs = g_lastLoadPath_Instruments = g_lastLoadPath_Tracks = "";
}

static INT_PTR RunConfig(QWidget* parent, COptionsDialog* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("RITMO configuration");
    auto* layout = new QVBoxLayout(&dialog);

    auto addCheck = [](QLayout* to, const char* text, BOOL on) {
        auto* check = new QCheckBox(text);
        check->setChecked(on);
        to->addWidget(check);
        return check;
    };
    auto spin = [](int min, int max, int value) {
        auto* box = new QSpinBox;
        box->setRange(min, max); // DDV_MinMaxInt
        box->setValue(value);
        return box;
    };

    // General
    auto* general = new QGroupBox("General");
    auto* generalLayout = new QVBoxLayout(general);
    auto* scaling = spin(100, 300, dlg->m_scaling_percentage);
    auto* primary = spin(2, 256, dlg->m_trackLinePrimaryHighlight);
    auto* secondary = spin(2, 256, dlg->m_trackLineSecondaryHighlight);
    auto* sizeRow = new QHBoxLayout;
    sizeRow->addWidget(new QLabel("Interface size (in %)"));
    sizeRow->addWidget(scaling);
    sizeRow->addSpacing(16);
    sizeRow->addWidget(new QLabel("Track line highlight step"));
    sizeRow->addWidget(primary);
    sizeRow->addWidget(new QLabel("/"));
    sizeRow->addWidget(secondary);
    sizeRow->addStretch();
    generalLayout->addLayout(sizeRow);
    auto* grid = new QGridLayout;
    auto gridCheck = [&](int row, int col, const char* text, BOOL on) {
        auto* check = new QCheckBox(text);
        check->setChecked(on);
        grid->addWidget(check, row, col);
        return check;
    };
    auto* german = gridCheck(0, 0, "Use German notation", dlg->m_usegermannotation);
    auto* altNumbering = gridCheck(0, 1, "Alternative track line numbering", dlg->m_tracklinealtnumbering);
    auto* flats = new QCheckBox("Display accidentals as Flats instead of Sharps");
    flats->setChecked(dlg->m_displayflatnotes);
    grid->addWidget(flats, 1, 0, 1, 2);
    auto* ntsc = gridCheck(2, 0, "NTSC system speed (60Hz)", dlg->m_ntsc);
    // The size of the pieces the sound output takes (CompatAudio.cpp); "Don't use hardware soundbuffer"
    // of RMT is not shown, it only meant something to DirectSound
    auto* audioBuffer = new QComboBox;
    const std::pair<int, const char*> bufferSizes[] = {
        { 5, "5 ms - lowest latency (live playing)" },
        { 10, "10 ms" },
        { 20, "20 ms" },
        { 30, "30 ms" },
        { 40, "40 ms - slow computers, crackling sound" },
    };
    for (const auto& size : bufferSizes) {
        QString text = size.second;
        if (size.first == RMT_DEFAULT_AUDIO_BUFFER_MS) text += " (default)";
        audioBuffer->addItem(text, size.first);
    }
    if (audioBuffer->findData(dlg->m_audioBufferMs) < 0) // another size written in ritmo.ini
        audioBuffer->addItem(QString("%1 ms").arg(dlg->m_audioBufferMs), dlg->m_audioBufferMs);
    audioBuffer->setCurrentIndex(audioBuffer->findData(dlg->m_audioBufferMs));
    audioBuffer->setToolTip("Larger: the sound does not crackle when the computer is busy, but it comes later after a key");
    auto* audioBufferRow = new QHBoxLayout;
    audioBufferRow->addWidget(new QLabel("Audio buffer:"));
    audioBufferRow->addWidget(audioBuffer, 1);
    grid->addLayout(audioBufferRow, 2, 1);
    auto* smooth = gridCheck(3, 0, "Smooth scroll during playback", dlg->m_doSmoothScrolling);
    generalLayout->addLayout(grid);
    auto* debug = addCheck(generalLayout, "Debug display (enable only if you know what you are doing)", dlg->m_viewDebugDisplay);
    generalLayout->addWidget(new QLabel("RMT Driver Version:"));
    auto* driver = new QComboBox;
    driver->addItems({ "No RMT Driver", "RMT 1.28 Unpatched by Raster", "RMT 1.28 Unpatched with Tuning",
                       "RMT 1.25 Patch3 Instrumentarium by Analmux", "RMT 1.27 Patch6 by Analmux", "RMT 1.28 Patch8 by Analmux",
                       "RMT 1.34 Patch16 by VinsCool", "RMT 1.28 Patch Prince of Persia by VinsCool" }); // index = TrackerDriverVersion
    driver->setCurrentIndex(dlg->m_trackerDriverVersion);
    generalLayout->addWidget(driver);
    layout->addWidget(general);

    // Keyboard
    auto* keyboard = new QGroupBox("Keyboard");
    auto* keyboardLayout = new QVBoxLayout(keyboard);
    auto* layoutCombo = new QComboBox;
    // Shown QWERTY, QWERTZ, AZERTY; the item data is the KeyboardLayout number kept in ritmo.ini
    layoutCombo->addItem("QWERTY Layout", (int)KeyboardLayout::QWERTY);
    layoutCombo->addItem("QWERTZ Layout", (int)KeyboardLayout::QWERTZ);
    layoutCombo->addItem("AZERTY Layout", (int)KeyboardLayout::AZERTY);
    layoutCombo->setCurrentIndex(std::max(0, layoutCombo->findData((int)dlg->m_keyboard_layout)));
    auto* layoutRow = new QHBoxLayout;
    layoutRow->addWidget(new QLabel("Keyboard layout:"));
    layoutRow->addWidget(layoutCombo, 1);
    keyboardLayout->addLayout(layoutRow);
    auto* upDown = addCheck(keyboardLayout, "Move to previous/next song line while track boundaries are crossed", dlg->m_keyboard_updowncontinue);
    auto* remember = addCheck(keyboardLayout, "Remember octaves and volumes separately for each instrument", dlg->m_keyboard_rememberoctavesandvolumes);
    auto* escReset = addCheck(keyboardLayout, "Reset the Atari sound routines each time ESC is pressed", dlg->m_keyboard_escresetatarisound);
    auto* askCtrlS = addCheck(keyboardLayout, "Prompt a save dialog box each time CTRL+S is pressed", dlg->m_keyboard_askwhencontrol_s);
    layout->addWidget(keyboard);

    // MIDI: device 0 of the combo is "none" (m_midi_device -1)
    auto* midi = new QGroupBox("MIDI");
    auto* midiLayout = new QVBoxLayout(midi);
    auto* device = new QComboBox;
    device->addItem("--- none ---");
    int numMidiDevices = midiInGetNumDevs();
    for (int i = 0; i < numMidiDevices; i++) {
        MIDIINCAPS micaps;
        midiInGetDevCaps(i, &micaps, sizeof(MIDIINCAPS));
        device->addItem(QString::fromLocal8Bit(micaps.szPname));
    }
    device->setCurrentIndex(std::max(0, std::min(dlg->m_midi_device + 1, device->count() - 1)));
    auto* deviceRow = new QHBoxLayout;
    deviceRow->addWidget(new QLabel("MIDI IN device:"));
    deviceRow->addWidget(device, 1);
    midiLayout->addLayout(deviceRow);
    auto* touch = new QCheckBox("Touch response");
    touch->setChecked(dlg->m_midi_TouchResponse);
    auto* volumeOffset = spin(0, 15, dlg->m_midi_VolumeOffset);
    auto* touchRow = new QHBoxLayout;
    touchRow->addWidget(touch);
    touchRow->addStretch();
    touchRow->addWidget(new QLabel("Atari volume offset"));
    touchRow->addWidget(volumeOffset);
    midiLayout->addLayout(touchRow);
    auto* noteOff = addCheck(midiLayout, "Record Note off", dlg->m_midi_NoteOff);
    // COptionsDialog::OnMidiTouchResponseClicked(): the offset needs touch response
    volumeOffset->setEnabled(touch->isChecked());
    QObject::connect(touch, &QCheckBox::toggled, volumeOffset, &QSpinBox::setEnabled);
    layout->addWidget(midi);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* paths = buttons->addButton("Paths...", QDialogButtonBox::ResetRole);
    QObject::connect(paths, &QPushButton::clicked, &dialog, [&dialog] { RunConfigPaths(&dialog); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;

    dlg->m_scaling_percentage = scaling->value();
    dlg->m_trackLinePrimaryHighlight = primary->value();
    dlg->m_trackLineSecondaryHighlight = secondary->value();
    dlg->m_usegermannotation = german->isChecked();
    dlg->m_tracklinealtnumbering = altNumbering->isChecked();
    dlg->m_displayflatnotes = flats->isChecked();
    dlg->m_ntsc = ntsc->isChecked();
    dlg->m_audioBufferMs = audioBuffer->currentData().toInt();
    dlg->m_doSmoothScrolling = smooth->isChecked();
    dlg->m_viewDebugDisplay = debug->isChecked();
    dlg->m_trackerDriverVersion = (TrackerDriverVersion)driver->currentIndex();
    dlg->m_keyboard_layout = (KeyboardLayout)layoutCombo->currentData().toInt();
    dlg->m_keyboard_updowncontinue = upDown->isChecked();
    dlg->m_keyboard_rememberoctavesandvolumes = remember->isChecked();
    dlg->m_keyboard_escresetatarisound = escReset->isChecked();
    dlg->m_keyboard_askwhencontrol_s = askCtrlS->isChecked();
    dlg->m_midi_device = device->currentIndex() - 1;
    dlg->m_midi_TouchResponse = touch->isChecked();
    dlg->m_midi_VolumeOffset = volumeOffset->value();
    dlg->m_midi_NoteOff = noteOff->isChecked();
    return IDOK;
}

// ---------------------------------------------------------------------------
// Song, track and instrument dialogs (effectsdlg.cpp, filenewdlg.cpp,
// importdlgs.cpp, TuningDialog.cpp)
// ---------------------------------------------------------------------------

// A hex number edit (ES_UPPERCASE), read with Hexstr() like the other dialogs
static QLineEdit* HexEdit(int value)
{
    auto* edit = UpperCaseEdit(QString::asprintf("%02X", value));
    edit->setMaximumWidth(40);
    return edit;
}

static int HexValue(QLineEdit* edit, int len)
{
    QByteArray text = edit->text().toLatin1();
    return Hexstr(text.data(), len);
}

static int IntValue(QLineEdit* edit) { return atoi(edit->text().toLatin1().constData()); }

// A choice between radio buttons ("Are you sure?"); choice gets the id of the
// chosen one: first, first + 1...
static INT_PTR RunChoice(QWidget* parent, const char* title, const QString& heading, const QStringList& options, int first, int& choice)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(heading));
    auto* group = new QButtonGroup(&dialog);
    for (int i = 0; i < options.size(); i++) {
        auto* radio = new QRadioButton(options[i]);
        group->addButton(radio, first + i);
        layout->addWidget(radio);
    }
    group->button(first)->setChecked(true); // OnInitDialog: the first one
    layout->addWidget(new QLabel("Are you sure?"));

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;
    choice = group->checkedId();
    return IDOK;
}

// IDD_CHANGEMAXTRACKLEN - CChangeMaxtracklenDlg
static INT_PTR RunChangeMaxTrackLen(QWidget* parent, CChangeMaxtracklenDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Change maximal length of tracks");
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Change maximal length of tracks"));
    layout->addWidget(new QLabel(FromCString(dlg->m_info)));

    auto* length = new QSpinBox;
    length->setRange(1, TRACKLEN); // DDV_MinMaxInt(1, TRACKLEN)
    length->setValue(dlg->m_maxtracklen);
    auto* row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(new QLabel("New maximal length of tracks"));
    row->addWidget(length);
    row->addStretch();
    layout->addLayout(row);

    auto* warning = new QLabel("Warning: All tracks will be prolonged or truncated!");
    warning->setAlignment(Qt::AlignCenter);
    layout->addWidget(warning);

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;
    dlg->m_maxtracklen = length->value();
    return IDOK;
}

// IDD_SONGINSERTCOPYORCLONEOFSONGLINES - CInsertCopyOrCloneOfSongLinesDlg
static INT_PTR RunInsertSongLines(QWidget* parent, CInsertCopyOrCloneOfSongLinesDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Insert copy or clone of song line(s) into song");
    auto* layout = new QVBoxLayout(&dialog);

    auto* from = HexEdit(dlg->m_linefrom);
    auto* to = HexEdit(dlg->m_lineto);
    auto* lineRow = new QHBoxLayout;
    lineRow->addWidget(new QLabel("Source songline(s): From line $"));
    lineRow->addWidget(from);
    lineRow->addWidget(new QLabel("to $"));
    lineRow->addWidget(to);
    lineRow->addStretch();
    layout->addLayout(lineRow);

    auto* info = new QLabel;
    info->setAlignment(Qt::AlignCenter);
    layout->addWidget(info);

    auto* clone = new QCheckBox("Cl&one tracks");
    clone->setChecked(dlg->m_clone);
    layout->addWidget(clone);
    auto* text1 = new QLabel("Tuning (+/- halftones):");
    auto* text2 = new QLabel("Volume (%):");
    auto* tuning = UpperCaseEdit(QString::number(dlg->m_tuning));
    auto* volume = UpperCaseEdit(QString::number(dlg->m_volumep));
    tuning->setMaximumWidth(40);
    volume->setMaximumWidth(40);
    auto* cloneRow = new QHBoxLayout;
    cloneRow->addStretch();
    cloneRow->addWidget(text1);
    cloneRow->addWidget(tuning);
    cloneRow->addSpacing(8);
    cloneRow->addWidget(text2);
    cloneRow->addWidget(volume);
    layout->addLayout(cloneRow);

    // OnChangeSonglinerange()
    auto changeRange = [&] {
        int f = HexValue(from, 4);
        int t = HexValue(to, 4);
        if (t < f) t = f;
        int n = t - f + 1;
        info->setText(n > 1 ? QString::asprintf("%i lines will be inserted into $%02X song line.", n, dlg->m_lineinto)
                            : QString::asprintf("1 line will be inserted into $%02X song line.", dlg->m_lineinto));
    };
    // ValuesTest(): the values are corrected, false if one needed it
    auto valuesTest = [&] {
        bool r = true;
        bool c = clone->isChecked();
        dlg->m_clone = c;
        for (QWidget* w : std::initializer_list<QWidget*>{ text1, text2, tuning, volume }) w->setEnabled(c);

        int v = HexValue(from, 4);
        if (v < 0) {
            v = 0;
            r = false;
        } else if (v >= SONGLEN) {
            v = SONGLEN - 1;
            r = false;
        }
        dlg->m_linefrom = v;
        from->setText(QString::asprintf("%02X", v));

        v = HexValue(to, 4);
        if (v < 0) {
            v = 0;
            r = false;
        } else if (v >= SONGLEN) {
            v = SONGLEN - 1;
            r = false;
        }
        if (v < dlg->m_linefrom) {
            v = dlg->m_linefrom;
            r = false;
        } // it can't be smaller
        dlg->m_lineto = v;
        to->setText(QString::asprintf("%02X", v));

        changeRange();
        dlg->m_tuning = IntValue(tuning);

        v = IntValue(volume);
        if (v < 0) {
            v = 0;
            r = false;
        } else if (v >= 1600) {
            v = 1600;
            r = false;
        }
        dlg->m_volumep = v;
        volume->setText(QString::number(v));
        return r;
    };
    QObject::connect(from, &QLineEdit::textChanged, &dialog, changeRange);
    QObject::connect(to, &QLineEdit::textChanged, &dialog, changeRange);
    QObject::connect(clone, &QCheckBox::clicked, &dialog, valuesTest);
    valuesTest();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto ok = [&] {
        if (!valuesTest()) {
            MessageBox(g_hwnd, "Some parameters need to be corrected.\nPlease re-verify their values.", "Warning", MB_ICONWARNING);
            return;
        }
        dialog.accept();
    };
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, ok);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    return ExecDialog(dialog, ok) ? IDOK : IDCANCEL;
}

// IDD_OCTAVESELECT, IDD_VOLUMESELECT, IDD_INSTRUMENTSELECT - the small tool
// windows of the info line. The other ones are placed at m_pos, the click point
// moved left by dx and up by 7; here from the mouse position, which is the
// click point on the screen
static void PlacePopup(QDialog& dialog, int dx)
{
    dialog.setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint); // WS_EX_TOOLWINDOW
    QPoint pos = QCursor::pos() - QPoint(dx, 7);
    dialog.move(std::max(0, pos.x()), std::max(0, pos.y()));
}

// COctaveSelectDlg: a button for each octave, the current one has the focus
static INT_PTR RunOctaveSelect(QWidget* parent, COctaveSelectDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Octave");
    PlacePopup(dialog, 64 + 9);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QPushButton* current = nullptr;
    for (int octave = 4; octave >= 0; octave--) {
        auto* button = new QPushButton(QString("%1 - %2").arg(octave + 1).arg(octave + 2));
        QObject::connect(button, &QPushButton::clicked, &dialog, [&dialog, dlg, octave] { // OnOctave()
            dlg->m_octave = octave;
            dialog.accept();
        });
        layout->addWidget(button);
        if (octave == dlg->m_octave) current = button;
    }
    if (current) current->setFocus();
    return ExecDialog(dialog, [current, &dialog] { if (current) current->click(); else dialog.accept(); }) ? IDOK : IDCANCEL;
}

// CVolumeSelectDlg: a click on a volume chooses it
static INT_PTR RunVolumeSelect(QWidget* parent, CVolumeSelectDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Volume");
    PlacePopup(dialog, 64 + 9);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* list = new QListWidget;
    list->setFont(MonoFont());
    for (int volume = 15; volume >= 0; volume--)
        list->addItem(QString::asprintf("%X  ", volume) + QString(volume, '|'));
    list->setCurrentRow(15 - dlg->m_volume);
    list->setMinimumHeight(list->sizeHintForRow(0) * 16 + 2 * list->frameWidth());
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(list);
    auto* respect = new QCheckBox("respect vol.");
    respect->setChecked(dlg->m_respectvolume);
    layout->addWidget(respect);
    auto* ok = new QPushButton("OK");
    ok->setDefault(true);
    layout->addWidget(ok);

    QObject::connect(list, &QListWidget::itemClicked, &dialog, &QDialog::accept); // OnSelchangeList1()
    QObject::connect(list, &QListWidget::itemActivated, &dialog, &QDialog::accept);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    list->setFocus();
    if (!ExecDialog(dialog, [ok] { ok->click(); })) return IDCANCEL;

    dlg->m_volume = 15 - list->currentRow();
    dlg->m_respectvolume = respect->isChecked();
    return IDOK;
}

// CInstrumentSelectDlg: a click on an instrument chooses it
static INT_PTR RunInstrumentSelect(QWidget* parent, CInstrumentSelectDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Instrument");
    PlacePopup(dialog, 64 + 82);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* list = new QListWidget;
    QFont font = MonoFont();
    font.setBold(true);
    list->setFont(font);
    for (int i = 0; i < INSTRSNUM; i++)
        list->addItem(QString::asprintf("%02X: ", i) + QString::fromLocal8Bit(g_Instruments.GetName(i)));
    QFontMetrics fm(font);
    list->setMinimumSize(fm.horizontalAdvance(QString(4 + INSTRUMENT_NAME_MAX_LEN, '0')) + 32, fm.lineSpacing() * 32);
    layout->addWidget(list);
    list->setCurrentRow(dlg->m_selected);
    list->scrollToItem(list->currentItem(), QAbstractItemView::PositionAtCenter);

    QObject::connect(list, &QListWidget::itemClicked, &dialog, &QDialog::accept); // OnSelchangeList1()
    QObject::connect(list, &QListWidget::itemActivated, &dialog, &QDialog::accept);
    list->setFocus();
    if (!ExecDialog(dialog, [&dialog] { dialog.accept(); })) return IDCANCEL;

    dlg->m_selected = list->currentRow();
    return IDOK;
}

// IDD_SONGTRACKSORDER - CSongTracksOrderDlg: the columns of the song are
// chosen by clicking a "From" button and then the "To" buttons; a line shows
// where each column comes from (CSongTracksOrderDlg::OnPaint())
class TracksOrderArea : public QWidget {
public:
    QPushButton* m_from[8] = {};
    QPushButton* m_to[8] = {};
    int m_order[8] = {};

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setPen(QPen(palette().color(QPalette::WindowText), 1));
        for (int i = 0; i < g_tracks4_8; i++) {
            int z = m_order[i];
            if (z < 0) continue;
            QRect s = m_from[z]->geometry(), d = m_to[i]->geometry();
            painter.drawLine(s.center().x(), s.bottom(), d.center().x(), d.top());
        }
    }
};

static INT_PTR RunSongTracksOrder(QWidget* parent, CSongTracksOrderDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Song columns' order change/copy/clear");
    auto* layout = new QVBoxLayout(&dialog);

    static const char* names[8] = { "L1", "L2", "L3", "L4", "R1", "R2", "R3", "R4" };
    auto* area = new TracksOrderArea;
    auto* grid = new QGridLayout(area);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->addWidget(new QLabel("From:"), 0, 0);
    grid->addWidget(new QLabel("To:"), 2, 0);
    grid->setRowMinimumHeight(1, 60);
    grid->setColumnMinimumWidth(5, 12);
    auto* fromGroup = new QButtonGroup(&dialog); // the checked one is m_fromtrack
    int fontWidth = QFontMetrics(dialog.font()).horizontalAdvance("R4");
    for (int i = 0; i < 8; i++) {
        int column = 1 + i + (i >= 4);
        auto* from = new QPushButton(names[i]);
        auto* to = new QPushButton(names[i]);
        for (QPushButton* b : { from, to }) b->setFixedWidth(fontWidth + 20);
        from->setCheckable(true);
        fromGroup->addButton(from, i);
        grid->addWidget(from, 0, column);
        grid->addWidget(to, 2, column);
        area->m_from[i] = from;
        area->m_to[i] = to;
        QObject::connect(to, &QPushButton::clicked, area, [area, fromGroup, i] { // OnL1R4()
            area->m_order[i] = fromGroup->checkedId() < 8 ? fromGroup->checkedId() : -1;
            area->update();
        });
    }
    auto* nothing = new QPushButton("Nothing"); // OnNothing(): the "To" buttons clear
    nothing->setCheckable(true);
    nothing->setChecked(true); // m_fromtrack = -1
    fromGroup->addButton(nothing, 8);
    grid->addWidget(nothing, 0, 11);
    grid->setColumnStretch(12, 1);
    layout->addWidget(area);

    auto setOrder = [area](std::function<int(int)> order) {
        for (int i = 0; i < 8; i++) area->m_order[i] = order(i);
        area->update();
    };
    auto* tools = new QGridLayout;
    auto addTool = [&](const char* text, int row, int column, std::function<int(int)> order) {
        auto* button = new QPushButton(text);
        QObject::connect(button, &QPushButton::clicked, area, [setOrder, order] { setOrder(order); });
        tools->addWidget(button, row, column);
        return button;
    };
    static const int monoStereo[8] = { 0, 3, 4, 7, 1, 2, 5, 6 };
    static const int stereoMono[8] = { 0, 4, 5, 1, 2, 6, 7, 3 };
    auto* toStereo = addTool("Mono-->stereo", 0, 0, [](int i) { return monoStereo[i]; }); // OnMonostereo()
    auto* toMono = addTool("Mono<--stereo", 1, 0, [](int i) { return stereoMono[i]; });   // OnStereomono()
    addTool("Default", 2, 0, [](int i) { return i; });                                    // OnDefault()
    auto* copyRight = addTool("Copy left-->right", 0, 1, [](int i) { return i % 4; });    // OnCopyleftright()
    auto* copyLeft = addTool("Copy left<--right", 1, 1, [](int i) { return i % 4 + 4; }); // OnCopyrightleft()
    addTool("Clear all", 2, 1, [](int) { return -1; });                                   // OnClearall()
    auto* from = UpperCaseEdit(FromCString(dlg->m_songlinefrom));
    auto* to = UpperCaseEdit(FromCString(dlg->m_songlineto));
    for (QLineEdit* edit : { from, to }) edit->setMaximumWidth(40);
    auto* fromLabel = new QLabel("From songline: $");
    auto* toLabel = new QLabel("To songline: $");
    fromLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    toLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tools->addWidget(fromLabel, 0, 2);
    tools->addWidget(from, 0, 3);
    tools->addWidget(toLabel, 1, 2);
    tools->addWidget(to, 1, 3);
    tools->setColumnStretch(2, 1);
    layout->addLayout(tools);
    setOrder([](int i) { return i; }); // OnInitDialog(): OnDefault()

    // Mono songs: no right channels
    if (g_tracks4_8 <= 4) {
        for (int i = 4; i < 8; i++) {
            area->m_from[i]->setEnabled(false);
            area->m_to[i]->setEnabled(false);
        }
        for (QPushButton* b : { toStereo, toMono, copyRight, copyLeft }) b->setEnabled(false);
    }

    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;

    for (int i = 0; i < 8; i++) dlg->m_tracksorder[i] = area->m_order[i];
    dlg->m_songlinefrom = ToCString(from->text());
    dlg->m_songlineto = ToCString(to->text());
    return IDOK;
}

// IDD_CHANNELSSELECT - CChannelsSelectionDlg: bit i of the result for channel
// i (L1-L4, R1-R4), -1 when cancelled
static int RunChannelsSelection(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Channels selection");
    auto* layout = new QVBoxLayout(&dialog);
    auto* row = new QHBoxLayout;
    QCheckBox* checks[8];
    static const char* names[8] = { "L1", "L2", "L3", "L4", "R1", "R2", "R3", "R4" };
    for (int side = 0; side < 2; side++) {
        auto* box = new QGroupBox(side ? "Right" : "Left");
        auto* boxLayout = new QHBoxLayout(box);
        for (int i = side * 4; i < side * 4 + 4; i++) {
            checks[i] = new QCheckBox(names[i]);
            checks[i]->setEnabled(i < 4 || g_tracks4_8 > 4);
            boxLayout->addWidget(checks[i]);
        }
        row->addWidget(box);
    }
    layout->addLayout(row);
    auto* buttons = AddOkCancel(dialog, layout);
    if (!ExecDialog(dialog, [&checks, buttons] { checks[0]->setChecked(true); ClickOk(buttons); })) return -1;

    int channels = 0;
    for (int i = 0; i < 8; i++)
        if (checks[i]->isChecked()) channels |= 1 << i;
    return channels;
}

// IDD_INSTRCHANGE - CInstrumentChangeDlg: the instrument, note and volume
// ranges of the condition (combos 11, 12, 1-4) and of the change (9, 10, 5-8)
static INT_PTR RunInstrumentChange(QWidget* parent, CInstrumentChangeDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Change all the instrument occurences");
    auto* layout = new QVBoxLayout(&dialog);
    auto* title = new QLabel;
    auto* title2 = new QLabel;
    layout->addWidget(title);
    layout->addWidget(title2);

    // c[i] is IDC_COMBO<i+1>; the "---" items are one past the last value
    QComboBox* c[12];
    for (auto& combo : c) combo = new QComboBox;
    for (int i = 0; i < CNotes::NOTESNUM; i++) {
        const char* note = CNotes::GetNote(i);
        for (int k : { 0, 1, 4, 5 }) c[k]->addItem(note);
    }
    c[5]->addItem("---");
    for (int i = 0; i <= 15; i++)
        for (int k : { 2, 3, 6, 7 }) c[k]->addItem(QString::asprintf("%X", i));
    c[7]->addItem("---");
    for (int i = 0; i < INSTRSNUM; i++)
        for (int k : { 8, 9, 10, 11 }) c[k]->addItem(QString::asprintf("%02X", i));
    c[9]->addItem("---");

    auto addRanges = [&](const char* text, std::initializer_list<int> combos) {
        auto* box = new QGroupBox(text);
        auto* form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        const char* labels[6] = { "From instr", "To instr", "From note", "To note", "Min volume", "Max volume" };
        int i = 0;
        for (int k : combos) form->addRow(labels[i++], c[k]);
        return box;
    };
    auto* ranges = new QHBoxLayout;
    ranges->addWidget(addRanges("Condition", { 10, 11, 0, 1, 2, 3 }));
    ranges->addWidget(addRanges("If true, change to", { 8, 9, 4, 5, 6, 7 }));
    layout->addLayout(ranges);

    auto* options = new QGridLayout;
    auto* oneInstr = new QCheckBox("One instrument only");
    auto* check4 = new QCheckBox("Only in current track");
    auto* check5 = new QCheckBox("Only in some channels");
    auto* check6 = new QCheckBox("Only in songlines");
    auto* check3 = new QCheckBox("The same instrument range");
    auto* check1 = new QCheckBox("The same note range");
    auto* check2 = new QCheckBox("The same volume range");
    options->addWidget(oneInstr, 0, 0);
    options->addWidget(check4, 1, 0);
    options->addWidget(check5, 2, 0);
    options->addWidget(check6, 3, 0);
    options->addWidget(check3, 0, 1);
    options->addWidget(check1, 1, 1);
    options->addWidget(check2, 2, 1);
    auto* edit1 = HexEdit(dlg->m_onlysonglinefrom);
    auto* edit2 = HexEdit(dlg->m_onlysonglineto);
    edit1->setEnabled(false);
    edit2->setEnabled(false);
    auto* songlines = new QHBoxLayout;
    songlines->addSpacing(16);
    songlines->addWidget(new QLabel("from $"));
    songlines->addWidget(edit1);
    songlines->addWidget(new QLabel("to $"));
    songlines->addWidget(edit2);
    songlines->addStretch();
    options->addLayout(songlines, 4, 0);
    layout->addLayout(options);

    if (dlg->m_onlytrack >= 0)
        check4->setText(QString::asprintf("Only in current track ($%02X)", dlg->m_onlytrack));
    else
        check4->setEnabled(false);
    dlg->m_onlychannels = -1;
    oneInstr->setChecked(true);
    int initial[12] = { dlg->m_combo1, dlg->m_combo2, dlg->m_combo3, dlg->m_combo4, dlg->m_combo5, dlg->m_combo6,
                        dlg->m_combo7, dlg->m_combo8, dlg->m_combo9, dlg->m_combo10, dlg->m_combo11, dlg->m_combo12 };
    for (int i = 0; i < 12; i++) c[i]->setCurrentIndex(initial[i]);

    // SelChangeComboX(): the "to" values are not below the "from" ones, and
    // the ranges of the change follow the ones of the condition if asked
    auto selChangeComboX = [&] {
        int v[12];
        for (int i = 0; i < 12; i++) v[i] = c[i]->currentIndex();
        if (v[1] < v[0]) v[1] = v[0];
        if (v[3] < v[2]) v[3] = v[2];
        if (v[5] < v[4]) v[5] = v[4];
        if (v[7] < v[6]) v[7] = v[6];
        if (v[9] < v[8]) v[9] = v[8];
        if (v[11] < v[10]) v[11] = v[10];
        if (check1->isChecked()) v[5] = std::min(v[1] - v[0] + v[4], (int)CNotes::NOTESNUM); // or "---"
        if (check2->isChecked()) v[7] = std::min(v[3] - v[2] + v[6], 16);
        if (check3->isChecked() || oneInstr->isChecked()) {
            if (oneInstr->isChecked()) v[11] = v[10];
            v[9] = std::min(v[11] - v[10] + v[8], (int)INSTRSNUM);
        }
        c[5]->setEnabled(!check1->isChecked());
        c[7]->setEnabled(!check2->isChecked());
        c[9]->setEnabled(!check3->isChecked() && !oneInstr->isChecked());
        c[11]->setEnabled(!oneInstr->isChecked());
        check3->setEnabled(!oneInstr->isChecked());
        for (int i = 0; i < 12; i++) c[i]->setCurrentIndex(v[i]);
    };

    // OnDefault(): the ranges in use by the instruments of the condition
    auto setDefault = [&] {
        int instrfrom = c[10]->currentIndex();
        int instrto = c[11]->currentIndex();
        if (oneInstr->isChecked() || instrto < instrfrom) instrto = instrfrom;

        TInstrInfo iinfo;
        g_Song.InstrInfo(instrfrom, &iinfo, instrto);
        if (!iinfo.count) {
            iinfo.minnote = 0;
            iinfo.maxnote = CNotes::NOTESNUM - 1;
            iinfo.minvol = 0;
            iinfo.maxvol = MAXVOLUME;
        }
        if (instrfrom == instrto)
            title->setText(QString::asprintf("Instrument: %02X\tName: ", instrfrom) + QString::fromLocal8Bit(g_Instruments.GetName(instrfrom)));
        else
            title->setText(QString::asprintf("Instruments %02X-%02X (%u)", instrfrom, instrto, instrto - instrfrom + 1));
        title2->setText(QString::asprintf("Used in %u tracks, globally %u times.", iinfo.usedintracks, iinfo.count));

        check1->setChecked(true);
        check2->setChecked(true);
        check3->setChecked(true);
        int v[12] = { iinfo.minnote, iinfo.maxnote, iinfo.minvol, iinfo.maxvol, iinfo.minnote, iinfo.maxnote,
                      iinfo.minvol, iinfo.maxvol, instrfrom, instrto, instrfrom, instrto };
        for (int i = 0; i < 12; i++) c[i]->setCurrentIndex(v[i]);
        selChangeComboX();
    };

    // OnFullRanges(): all the instruments in use
    auto fullRanges = [&] {
        check1->setChecked(true);
        check2->setChecked(true);
        check3->setChecked(true);
        oneInstr->setChecked(false);
        TInstrInfo iinfo;
        g_Song.InstrInfo(0, &iinfo, INSTRSNUM - 1);
        if (!iinfo.count) {
            int v[12] = { 0, CNotes::NOTESNUM - 1, 0, 15, 0, CNotes::NOTESNUM - 1, 0, 15, 0, INSTRSNUM - 1, 0, INSTRSNUM - 1 };
            for (int i = 0; i < 12; i++) c[i]->setCurrentIndex(v[i]);
        } else {
            c[10]->setCurrentIndex(iinfo.instrfrom);
            c[11]->setCurrentIndex(iinfo.instrto);
        }
        setDefault();
    };

    // Only the user's choices, like CBN_SELCHANGE and BN_CLICKED
    for (int i = 0; i < 12; i++)
        QObject::connect(c[i], QOverload<int>::of(&QComboBox::activated), &dialog, i >= 10 ? std::function<void()>(setDefault) : std::function<void()>(selChangeComboX));
    for (QCheckBox* check : { check1, check2, check3 })
        QObject::connect(check, &QCheckBox::clicked, &dialog, selChangeComboX);
    QObject::connect(oneInstr, &QCheckBox::clicked, &dialog, [&](bool on) { // OnCheckoneinstrument()
        if (on)
            setDefault();
        else
            selChangeComboX();
    });
    // Only in current track excludes only in some channels / songlines
    QObject::connect(check4, &QCheckBox::clicked, &dialog, [&](bool on) { // OnCheckTrackOnly()
        if (!on) return;
        check5->setChecked(false);
        check5->setText("Only in some channels");
        dlg->m_onlychannels = -1;
        check6->setChecked(false);
        edit1->setEnabled(false);
        edit2->setEnabled(false);
    });
    QObject::connect(check5, &QCheckBox::clicked, &dialog, [&](bool on) { // OnCheckSomeChannelsOnly()
        if (on) {
            check4->setChecked(false);
            int channels = RunChannelsSelection(&dialog);
            if (channels > 0) {
                dlg->m_onlychannels = channels;
                QStringList names;
                static const char* cnames[8] = { "L1", "L2", "L3", "L4", "R1", "R2", "R3", "R4" };
                for (int i = 0; i < g_tracks4_8; i++)
                    if (channels & (1 << i)) names << cnames[i];
                check5->setText("Only in " + names.join(','));
            } else
                check5->setChecked(false);
        }
        if (!check5->isChecked()) {
            check5->setText("Only in some channels");
            dlg->m_onlychannels = -1;
        }
    });
    QObject::connect(check6, &QCheckBox::clicked, &dialog, [&](bool on) { // OnCheckSomeSonglinesOnly()
        if (on) check4->setChecked(false);
        edit1->setEnabled(on);
        edit2->setEnabled(on);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* defaultRanges = buttons->addButton("Default ranges", QDialogButtonBox::ResetRole);
    QPushButton* allInstruments = buttons->addButton("All instruments", QDialogButtonBox::ResetRole);
    QObject::connect(defaultRanges, &QPushButton::clicked, &dialog, setDefault);
    QObject::connect(allInstruments, &QPushButton::clicked, &dialog, fullRanges);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    setDefault();

    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) return IDCANCEL;

    // CInstrumentChangeDlg::OnOK() and its DDX
    if (!check4->isChecked()) dlg->m_onlytrack = -1;
    if (!check5->isChecked()) dlg->m_onlychannels = -1;
    if (check6->isChecked()) {
        dlg->m_onlysonglinefrom = HexValue(edit1, 4);
        dlg->m_onlysonglineto = HexValue(edit2, 4);
    } else
        dlg->m_onlysonglinefrom = dlg->m_onlysonglineto = -1;
    int* combos[12] = { &dlg->m_combo1, &dlg->m_combo2, &dlg->m_combo3, &dlg->m_combo4, &dlg->m_combo5, &dlg->m_combo6,
                        &dlg->m_combo7, &dlg->m_combo8, &dlg->m_combo9, &dlg->m_combo10, &dlg->m_combo11, &dlg->m_combo12 };
    for (int i = 0; i < 12; i++) *combos[i] = c[i]->currentIndex();
    return IDOK;
}

// IDD_EFFECTS - CEffectsDlg: effects on the selected block of a track. Try
// applies them to the track (from its original data), Restore and Cancel put
// the original data back
struct TEffect {
    const char* name;
    const char* p[3]; // parameter texts ("" = not used)
    const char* e[3]; // default values
};

static const TEffect s_effects[] = {
    { "Fade in/out", { "Initial volume level, 0-100 (%)", "Final volume level, 0-100 (%)", "Line step" }, { "100", "100", "1" } },
    { "Modify notes, instruments and volume values", { "Notes tuning (+- semitones)", "Instruments used (+- offset value)", "Volume changes (%)" }, { "0", "0", "100" } },
    { "Echo", { "Delay (lines)", "Fade out level 0-100 (%), or V1-V15 for linear volume subtraction", "Minimal volume 0-15, or !0-!15 for ending echo on minimal volume" }, { "3", "20", "1" } },
    { "Expand/shrink lines", { "From step (negative values for bottom-up way)", "To step (negative values for bottom-up way)", "" }, { "1", "2", "" } },
    { "Volume humanize", { "Random level 0-100 (%)", "Minimal volume 0-15", "Line step" }, { "30", "1", "1" } },
    { "Volume set/remove", { "Volume range - minimum 0-15", "Volume range - maximum 0-15", "Set volume to 0-15, or 'X' to remove whole note events" }, { "0", "15", "15" } },
};
static const int NUMBEROFEFFECTS = sizeof(s_effects) / sizeof(s_effects[0]);

static int s_effectIndex = 0;                      // g_effai: the last effect used
static QString s_effectParams[NUMBEROFEFFECTS][3]; // eff_ed: the last parameters of each effect

// ZpracujChPar(): "15", "-5", "E10", "E -5", "X"... split into the first
// letter (upper case) and the number
static void ProcessChPar(const QString& text, char& ch, int& par)
{
    QByteArray s = text.toLatin1();
    ch = 0;
    par = 0;
    for (int i = 0; i < s.size(); i++) {
        char a = s[i];
        if (a >= 'a' && a <= 'z') a -= 'a' - 'A';
        if ((a >= '0' && a <= '9') || a == '-') {
            par = atoi(s.constData() + i);
            return;
        }
        if (ch == 0 && a != ' ') ch = a;
    }
}

// CEffectsDlg::PerformEffect()
static void PerformEffect(CEffectsDlg* dlg, int effect, const QString params[3])
{
    int i, j, h;
    int bfro = dlg->m_bfro;
    int bto = dlg->m_bto;
    int p1, p2, p3;
    char ch1, ch2, ch3;
    ProcessChPar(params[0], ch1, p1);
    ProcessChPar(params[1], ch2, p2);
    ProcessChPar(params[2], ch3, p3);
    (void)ch1;

    TTrack td;
    memcpy(&td, dlg->m_trackorig, sizeof(TTrack));

    float fvolume[TRACKLEN]; // volume in real numbers
    for (i = bfro; i <= bto; i++) fvolume[i] = (float)td.volume[i];

    int continstr[TRACKLEN]; // the instrument numbers are continuous
    int lasti = -1;
    for (i = 0; i <= bto; i++) {
        if (td.instr[i] >= 0) lasti = td.instr[i];
        if (i >= bfro) continstr[i] = lasti;
    }

    TTrack tempt; // auxiliary empty track
    for (i = 0; i < TRACKLEN; i++) tempt.note[i] = tempt.instr[i] = tempt.volume[i] = tempt.speed[i] = -1;

    switch (effect) {
        case 0: // fade in/out: initial volume level %, final vol.level %, line step
            if (p3 <= 0) break;
            for (i = bfro; i <= bto; i += p3) {
                if (!dlg->m_all && dlg->m_ainstr != continstr[i]) continue;
                if (td.volume[i] < 0) continue; // never without volume
                float proc = (float)p1 / 100;
                if (i > 0) proc += (float)(p2 - p1) / (bto - bfro) * (i - bfro) / 100;
                h = (int)(proc * td.volume[i] + 0.5); // volume change (rounded)
                if (h < 0)
                    h = 0;
                else if (h > 15)
                    h = 15;
                td.volume[i] = h;
            }
            break;

        case 1: // change notes, instruments and volumes: note+-, instr+-, volume%
            g_Tracks.ModifyTrack(&td, bfro, bto, dlg->m_all ? -1 : dlg->m_ainstr, p1, p2, p3);
            break;

        case 2: { // echo: delay, fadeout level %, minimal volume 0..15 or !1..!15 echo ending volume
            float dvol = 0;
            if (ch2 == 'V') { // linear calculations
                if (p2 < -15) p2 = -15;
                if (p2 > 15) p2 = 15;
            } else { // percentage calculations
                dvol = (1 - ((float)p2 / 100));
                if (dvol < -15) dvol = -15;
                if (dvol > 15) dvol = 15;
            }
            for (i = bfro; i <= bto; i++) {
                if (td.note[i] < 0) continue; // there is no note
                if (!dlg->m_all && td.instr[i] != dlg->m_ainstr) continue;
                j = i + p1;                        // echo for p1
                if (j < bfro || j > bto) continue; // echo is coming out of the block
                if (td.note[j] >= 0) continue;     // there is already a note in the final place

                float nv = (ch2 == 'V') ? fvolume[i] - p2 : fvolume[i] * dvol;
                int ph = (int)(fvolume[i] + 0.5);     // original volume (rounded to the nearest)
                h = (int)(nv + 0.5);                  // new volume (rounded to the nearest)
                if (ch3 == '!' && ph <= p3) continue; // ending volume
                if (h < p3) h = p3;                   // minimal volume p3
                if (h < 0)
                    h = 0;
                else if (h > 15)
                    h = 15;

                fvolume[j] = nv;
                td.note[j] = td.note[i];
                td.instr[j] = td.instr[i];
                td.volume[j] = h;
            }
            break;
        }

        case 3: { // expand/shrink lines: from step, to step
            if (p1 == 0 && p2 == 0) break;
            int lenb = bto - bfro;
            for (i = (p1 >= 0) ? 0 : lenb, j = (p2 >= 0) ? 0 : lenb; i >= 0 && i <= lenb && j >= 0 && j <= lenb; i += p1, j += p2) {
                if (!dlg->m_all && td.instr[bfro + i] != dlg->m_ainstr) continue;
                tempt.note[j] = td.note[bfro + i];
                tempt.instr[j] = td.instr[bfro + i];
                tempt.volume[j] = td.volume[bfro + i];
                tempt.speed[j] = td.speed[bfro + i];
            }
            for (i = 0; i <= lenb; i++) {
                td.note[bfro + i] = tempt.note[i];
                td.instr[bfro + i] = tempt.instr[i];
                td.volume[bfro + i] = tempt.volume[i];
                td.speed[bfro + i] = tempt.speed[i];
            }
            break;
        }

        case 4: // volume humanize: random level %, minimal volume, line step
            if (p1 < 0) p1 = 0;
            if (p1 > 100) p1 = 100;
            if (p2 < 0) p2 = 0;
            if (p3 <= 0) break;
            for (i = bfro; i <= bto; i += p3) {
                int vol = td.volume[i];
                if (!dlg->m_all && dlg->m_ainstr != continstr[i]) continue;
                if (vol < 0) continue; // if there is no volume

                float dol = (vol - (float)p1 / 100 * 15);
                if (dol < p2) dol = (float)p2;
                dol -= 0.5;
                float hor = (vol + (float)p1 / 100 * 15);
                if (hor > 15) hor = 15;
                hor += 0.5;
                if (hor <= dol) continue;

                int nv = (int)(0.5 + dol + (float)((hor - dol) * (((float)(rand() % 1000)) / 1000)));
                if (nv < p2) nv = p2;
                if (nv > 15) nv = 15;
                td.volume[i] = nv;
            }
            break;

        case 5: // volume set / remove (ch3 == 'X')
            if (p1 < 0) p1 = 0;
            if (p1 > 15) p1 = 15;
            if (p2 < 0) p2 = 0;
            if (p2 > 15) p2 = 15;
            if (p3 < 0) p3 = 0;
            if (p3 > 15) p3 = 15;
            for (i = bfro; i <= bto; i++) {
                int vol = td.volume[i];
                if (!dlg->m_all && dlg->m_ainstr != continstr[i]) continue;
                if (vol < 0) continue;
                if (p1 <= vol && vol <= p2) {
                    if (ch3 == 'X')
                        td.note[i] = td.instr[i] = td.volume[i] = -1;
                    else
                        td.volume[i] = p3;
                }
            }
            break;
    }

    memcpy(dlg->m_trackptr, &td, sizeof(TTrack)); // copies to the actual track
    SCREENUPDATE;
}

static INT_PTR RunEffects(QWidget* parent, CEffectsDlg* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Effects/tools");
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(FromCString(dlg->m_info)));

    auto* combo = new QComboBox;
    for (const TEffect& effect : s_effects) combo->addItem(effect.name);
    layout->addWidget(combo);
    QLabel* label[3];
    QLineEdit* edit[3];
    for (int i = 0; i < 3; i++) {
        label[i] = new QLabel;
        edit[i] = UpperCaseEdit("");
        edit[i]->setMaximumWidth(80);
        layout->addWidget(label[i]);
        layout->addWidget(edit[i]);
    }

    memcpy(dlg->m_trackorig, dlg->m_trackptr, sizeof(TTrack)); // OnInitDialog()
    int effect = s_effectIndex;

    auto setDefault = [&] { // OnDefault()
        for (int i = 0; i < 3; i++) {
            edit[i]->setText(s_effects[effect].e[i]);
            s_effectParams[effect][i] = s_effects[effect].e[i];
        }
    };
    auto changeEffect = [&](int index) { // OnSelchangeEffCombo()
        effect = index;
        for (int i = 0; i < 3; i++) {
            label[i]->setText(s_effects[effect].p[i]);
            edit[i]->setEnabled(i == 0 || s_effects[effect].p[i][0] != 0);
        }
        if (s_effectParams[effect][0].isEmpty())
            setDefault(); // P1 empty: all defaults
        else
            for (int i = 0; i < 3; i++) edit[i]->setText(s_effectParams[effect][i]);
    };
    auto perform = [&] {
        for (int i = 0; i < 3; i++) s_effectParams[effect][i] = edit[i]->text();
        PerformEffect(dlg, effect, s_effectParams[effect]);
    };
    auto restore = [dlg] { // OnRestore()
        memcpy(dlg->m_trackptr, dlg->m_trackorig, sizeof(TTrack));
        SCREENUPDATE;
    };
    combo->setCurrentIndex(effect);
    changeEffect(effect);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), &dialog, changeEffect);

    auto* defaultButton = new QPushButton("Default");
    QObject::connect(defaultButton, &QPushButton::clicked, &dialog, setDefault);
    layout->addWidget(defaultButton, 0, Qt::AlignRight);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* tryButton = buttons->addButton("&Try", QDialogButtonBox::ActionRole);
    QPushButton* restoreButton = buttons->addButton("&Restore", QDialogButtonBox::ActionRole);
    QPushButton* playButton = buttons->addButton("&Play/Stop", QDialogButtonBox::ActionRole);
    QObject::connect(tryButton, &QPushButton::clicked, &dialog, perform);
    QObject::connect(restoreButton, &QPushButton::clicked, &dialog, restore);
    QObject::connect(playButton, &QPushButton::clicked, &dialog, [] { // OnPlaystop()
        if (g_Song.GetPlayMode())
            g_Song.Stop();
        else
            g_Song.Play(PLAY_BLOCK, g_Song.GetFollowPlayMode());
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (!ExecDialog(dialog, [buttons] { ClickOk(buttons); })) {
        restore(); // OnCancel()
        return IDCANCEL;
    }
    perform(); // OnOK()
    s_effectIndex = effect;
    return IDOK;
}

// IDD_TUNING - TuningDialog: Test now applies the values, Reset puts back the
// ones the dialog was opened with, which Cancel also does
static INT_PTR RunTuning(QWidget* parent, TuningDialog* dlg)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Tuning configuration");
    auto* layout = new QVBoxLayout(&dialog);

    const TTuningSettings settingsBackup = g_tuning;
    const TTuningRatios ratiosBackup = g_tuningRatios;

    auto* general = new QGroupBox("General");
    auto* generalLayout = new QGridLayout(general);
    auto* baseTuning = new QLineEdit;
    auto* baseNote = new QComboBox;
    baseNote->addItems({ "C-", "B-", "A#", "A-", "G#", "G-", "F#", "F-", "E-", "D#", "D-", "C#" }); // index = basenote
    auto* temperament = new QComboBox;
    temperament->addItems({ "Equal Temperament (Default)",
                            "Thomas Young 1799's Well Temperament no.1",
                            "Thomas Young 1799's Well Temperament no.2",
                            "Thomas Young 1807's Well Temperament",
                            "Andreas Werckmeister's Temperament III (1681)",
                            "Tempérament Égal a Quintes Justes",
                            "d'Alembert and Rousseau Tempérament Ordinaire (1752/1767)",
                            "Aron - Neidhardt Equal Beating Well Temperament",
                            "Atom Schisma Scale",
                            "12-TET Approximation with Minimal Order 17 Beats",
                            "Paul Bailey's Modern Well Temperament (2002)",
                            "John Barnes' Temperament (1977) Made After Analysis of Wohltemperierte Klavier",
                            "Bethisy Tempérament Ordinaire",
                            "Big Gulp",
                            "12 Tone Scale by Bohlen Generated from the 4:7 : 10 Triad, Acustica 39/2/1978",
                            "This Scale May Also be Called the \"Wedding Cake\"",
                            "Upside Down Wedding Cake (Divorce Cake)",
                            "12 Tone Pythagorean Scale",
                            "Robert Schneider, Scale of Log(4) ..Log(16)",
                            "Zarlino Tempérament Extraordinaire",
                            "Fokker's 7-Limit 12-Tone Just Scale",
                            "Bach Temperament, A- = 400hz",
                            "Vallotti & Young Scale (Vallotti Version), Also Known as Tartini-Vallotti (1754)",
                            "Vallotti-Young and Werckmeister III, 10 Cents 5-Limit Lesfip Scale",
                            "Optimally Consonant Major Pentatonic, John deLaubenfels (2001)",
                            "Ancient Greek Aeolic, Also Tritriaic Scale of the 54:64 : 81 Triad",
                            "African Bapare Xylophone (Idiophone, Loose Log)",
                            "African Yaswa Xylophone (Idiophone, Calbas Resonators with Membrane)",
                            "19-EDO Generated Using Scale Workshop",
                            "Custom Temperament with RA/TIO" }); // index = temperament (IDD_TUNING DLGINIT)
    temperament->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    temperament->setMinimumContentsLength(40);
    baseTuning->setMaximumWidth(160);
    generalLayout->addWidget(new QLabel("Base tuning (Hz)"), 0, 0);
    generalLayout->addWidget(baseTuning, 0, 1);
    generalLayout->addWidget(new QLabel("Base note"), 0, 2);
    generalLayout->addWidget(baseNote, 0, 3);
    generalLayout->addWidget(new QLabel("eg: 440, 440.83751645933, 432, 443.9, 444.895778867913, etc"), 1, 0, 1, 4);
    generalLayout->addWidget(new QLabel("Temperament"), 2, 0);
    generalLayout->addWidget(temperament, 2, 1, 1, 3);
    layout->addWidget(general);

    // The 13 ratios, numerator / denominator (ES_NUMBER)
    static const char* intervals[13] = { "Unison", "Minor 2nd", "Major 2nd", "Minor 3rd", "Major 3rd", "Perfect 4th",
                                         "Tritone", "Perfect 5th", "Minor 6th", "Major 6th", "Minor 7th", "Major 7th", "Octave" };
    auto ratioList = [](TTuningRatios& r) {
        return std::array<CFraction*, 13>{ &r.UNISON, &r.MIN_2ND, &r.MAJ_2ND, &r.MIN_3RD, &r.MAJ_3RD, &r.PERF_4TH,
                                           &r.TRITONE, &r.PERF_5TH, &r.MIN_6TH, &r.MAJ_6TH, &r.MIN_7TH, &r.MAJ_7TH, &r.OCTAVE };
    };
    auto* ratios = new QGroupBox("RA/TIO");
    auto* ratiosLayout = new QGridLayout(ratios);
    QLineEdit* numerator[13];
    QLineEdit* denominator[13];
    for (int i = 0; i < 13; i++) {
        numerator[i] = new QLineEdit;
        denominator[i] = new QLineEdit;
        for (QLineEdit* edit : { numerator[i], denominator[i] }) {
            edit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]{0,9}"), edit));
            edit->setMaximumWidth(60);
        }
        ratiosLayout->addWidget(new QLabel(intervals[i]), i, 0);
        ratiosLayout->addWidget(numerator[i], i, 1);
        ratiosLayout->addWidget(new QLabel("/"), i, 2);
        ratiosLayout->addWidget(denominator[i], i, 3);
    }
    ratiosLayout->setColumnStretch(4, 1);
    layout->addWidget(ratios);

    auto show = [&](const TTuningSettings& settings, TTuningRatios r) {
        baseTuning->setText(QString::number(settings.basetuning, 'g', 15));
        baseNote->setCurrentIndex(settings.basenote);
        temperament->setCurrentIndex(settings.temperament);
        auto list = ratioList(r);
        for (int i = 0; i < 13; i++) {
            numerator[i]->setText(QString::number(list[i]->numerator));
            denominator[i]->setText(QString::number(list[i]->denominator));
        }
    };
    show(dlg->m_tuningSettings, dlg->m_tuningRatios);

    // OnClickedIdtestnow(): the values of the dialog (DDV_MinMaxDouble for the
    // base tuning) become the tuning in use
    auto testNow = [&] {
        bool ok = false;
        double tuning = baseTuning->text().toDouble(&ok);
        if (!ok || tuning < 6.875 || tuning > 7040) {
            MessageBox(g_hwnd, "Please enter a number between 6.875 and 7040.", "Tuning configuration", MB_ICONEXCLAMATION);
            baseTuning->setFocus();
            return false;
        }
        dlg->m_tuningSettings.basetuning = tuning;
        dlg->m_tuningSettings.basenote = baseNote->currentIndex();
        dlg->m_tuningSettings.temperament = temperament->currentIndex();
        auto list = ratioList(dlg->m_tuningRatios);
        for (int i = 0; i < 13; i++) {
            list[i]->numerator = IntValue(numerator[i]);
            list[i]->denominator = IntValue(denominator[i]);
        }
        g_tuning = dlg->m_tuningSettings;
        g_tuningRatios = dlg->m_tuningRatios;
        g_Tuning.InitTuning();
        return true;
    };
    // OnClickedIdreset(): the values the dialog was opened with, also shown
    auto reset = [&] {
        g_tuning = settingsBackup;
        g_tuningRatios = ratiosBackup;
        g_Tuning.InitTuning();
        show(settingsBackup, ratiosBackup);
    };

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* test = buttons->addButton("Test now", QDialogButtonBox::ActionRole);
    QPushButton* resetButton = buttons->addButton("Reset", QDialogButtonBox::ResetRole);
    QObject::connect(test, &QPushButton::clicked, &dialog, testNow);
    QObject::connect(resetButton, &QPushButton::clicked, &dialog, reset);
    auto ok = [&] { if (testNow()) dialog.accept(); }; // OnOK()
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, ok);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (!ExecDialog(dialog, ok)) {
        reset(); // OnBnClickedCancel()
        return IDCANCEL;
    }
    return IDOK;
}

// ---------------------------------------------------------------------------

INT_PTR RmtQtRunDialog(QWidget* parent, CDialog* dlg)
{
    switch (dlg->m_nIDTemplate) {
        case IDD_FILENEW: return RunFileNew(parent, static_cast<CFileNewDlg*>(dlg));
        case IDD_CONFIG: return RunConfig(parent, static_cast<COptionsDialog*>(dlg));
        case IDD_IMPORTMOD: return RunImportMod(parent, static_cast<CImportModDlg*>(dlg));
        case IDD_EXPORT_STRIPPED_RMT: return RunExportStrippedRmt(parent, static_cast<CExportStrippedRMTDialog*>(dlg));
        case IDD_EXPMSX: return RunExportXex(parent, static_cast<CExpMSXDlg*>(dlg));
        case IDD_EXPORT_ASM: return RunExportAsm(parent, static_cast<CExportAsmDlg*>(dlg));
        case IDD_EXPORT_RMTPLAYER_ASM: return RunExportRelocatableAsm(parent, static_cast<CExportRelocatableAsmForRmtPlayer*>(dlg));
        case IDD_EXPSAP: return RunExportSap(parent, static_cast<CSAPFileExportDialog*>(dlg));
        case IDD_IMPORTTMC: return RunImportTmc(parent, static_cast<CImportTmcDlg*>(dlg));
        case IDD_IMPORTMODFINISHED:
            return RunImportFinished(parent, "Import ProTracker Module", static_cast<CImportModFinishedDlg*>(dlg)->m_info,
                                     "Please, now you have to look over all the instuments and set up proper envelopes distortions (drums, basses, etc.), also you must correct instruments tunings according to the tuning of original samples and improve them (chords, effects, noises, etc.).",
                                     g_importModUnderstood);
        case IDD_IMPORTTMCFINISHED:
            return RunImportFinished(parent, "Import Theta Music Composer module", static_cast<CImportTmcFinishedDlg*>(dlg)->m_info,
                                     "Please, now you have to look over all the instuments and whole song and check if it's all right, otherwise you must correct it manually (some special instrument effects aren't converted automatically). Also some stereo and AUDCTL events may be wrong, because of different stereo and AUDCTL conception in TMC and RMT.",
                                     g_importTmcUnderstood);
        case IDD_EFFECTS: return RunEffects(parent, static_cast<CEffectsDlg*>(dlg));
        case IDD_OCTAVESELECT: return RunOctaveSelect(parent, static_cast<COctaveSelectDlg*>(dlg));
        case IDD_VOLUMESELECT: return RunVolumeSelect(parent, static_cast<CVolumeSelectDlg*>(dlg));
        case IDD_INSTRUMENTSELECT: return RunInstrumentSelect(parent, static_cast<CInstrumentSelectDlg*>(dlg));
        case IDD_SONGTRACKSORDER: return RunSongTracksOrder(parent, static_cast<CSongTracksOrderDlg*>(dlg));
        case IDD_INSTRCHANGE: return RunInstrumentChange(parent, static_cast<CInstrumentChangeDlg*>(dlg));
        case IDD_SONGINSERTCOPYORCLONEOFSONGLINES: return RunInsertSongLines(parent, static_cast<CInsertCopyOrCloneOfSongLinesDlg*>(dlg));
        case IDD_CHANGEMAXTRACKLEN: return RunChangeMaxTrackLen(parent, static_cast<CChangeMaxtracklenDlg*>(dlg));
        case IDD_TUNING: return RunTuning(parent, static_cast<TuningDialog*>(dlg));
        case IDD_RENUMBERTRACKS: {
            auto* d = static_cast<CRenumberTracksDlg*>(dlg);
            return RunChoice(parent, "Renumber all tracks", "Renumber all tracks:",
                             { "Order by songcolumns at first.", "Order by songlines at first." }, 1, d->m_radio);
        }
        case IDD_RENUMBERINSTRUMENTS: {
            auto* d = static_cast<CRenumberInstrumentsDlg*>(dlg);
            return RunChoice(parent, "Renumber all instruments", "Renumber all instruments:",
                             { "No order change. Remove gaps between instrument slots.", "Order by use in tracks.",
                               "Order by instrument names (alphabetical)." },
                             1, d->m_radio);
        }
        case IDD_TRACKSLOAD: {
            auto* d = static_cast<CTracksLoadDlg*>(dlg);
            return RunChoice(parent, "Tracks loading", QString::asprintf("There are %u tracks in TXT file.", d->m_tracknum),
                             { QString::asprintf("Load tracks to $%02X-$%02X.", d->m_trackfrom, d->m_trackfrom + d->m_tracknum - 1),
                               "Load tracks to their original places stored in TXT file." },
                             0, d->m_radio);
        }
        default: return IDCANCEL;
    }
}
