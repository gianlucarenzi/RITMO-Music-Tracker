// RmtQtFrontend.cpp - Qt frontend of RITMO (see RmtQtFrontend.h)

#include "StdAfx.h"
#include "resource.h"
#include "RmtDoc.h"
#include "RmtView.h"
#include "MainFrm.h"
#include "Global.h"
#include "GuiHelpers.h"
#include "Song.h"

#include "RmtQtFrontend.h"
#include "RmtQtSettings.h"
#include "ScriptMessages.h"
#include "ActionTable.h"
#include "PokeyController.h"
#include "RmtMenus.h"
#include "RmtQtKeys.h"
#include "RmtQtDialogs.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QStatusBar>
#include <QToolBar>
#include <QPageSetupDialog>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <set>
#include <vector>

extern CStatusBar* g_statusBar;
extern CSong g_Song;

// ---------------------------------------------------------------------------
// The tracker classes, with their protected constructors and handlers opened up
// for the frontend
// ---------------------------------------------------------------------------

class QtRmtDoc : public CRmtDoc {
public:
    QtRmtDoc() {}
};

class QtMainFrame : public CMainFrame {
public:
    QtMainFrame() {}
};

class QtRmtView : public CRmtView {
public:
    QtRmtView() {}
    using CRmtView::OnTimer;
    using CRmtView::OnDestroy;
    using CRmtView::OnKeyDown;
    using CRmtView::OnKeyUp;
    using CRmtView::OnSysChar;
    using CRmtView::OnLButtonDown;
    using CRmtView::OnLButtonUp;
    using CRmtView::OnLButtonDblClk;
    using CRmtView::OnRButtonDown;
    using CRmtView::OnRButtonUp;
    using CRmtView::OnRButtonDblClk;
    using CRmtView::OnMouseMove;
    using CRmtView::OnMouseWheel;
    using CRmtView::OnSetFocus;
    using CRmtView::OnKillFocus;
};

// ---------------------------------------------------------------------------
// QtCCmdUI - bridges CCmdUI to QAction for ON_UPDATE_COMMAND_UI handlers
// ---------------------------------------------------------------------------

class QtCCmdUI : public CCmdUI {
public:
    QAction* m_action;
    QtCCmdUI(UINT id, QAction* action)
    {
        m_nID = id;
        m_action = action;
    }
    void Enable(BOOL on) override { m_action->setEnabled(on != FALSE); }
    void SetCheck(int check) override
    {
        // setCheckable(true) is set at construction for known toggle items;
        // calling it here during aboutToShow emits QAction::changed → menu repaint.
        if (m_action->isCheckable())
            m_action->setChecked(check != 0);
    }
    void SetRadio(BOOL on) override
    {
        if (m_action->isCheckable())
            m_action->setChecked(on != FALSE);
    }
    void SetText(LPCTSTR text) override
    {
        CCmdUI::SetText(text);
        if (text) m_action->setText(text);
    }
};

// A toolbar button: it becomes a toggle when its handler checks it (no open
// menu to repaint here), and keeps its icon whatever text the handler sets
class QtToolCmdUI : public QtCCmdUI {
public:
    using QtCCmdUI::QtCCmdUI;
    void SetCheck(int check) override
    {
        m_action->setCheckable(true);
        m_action->setChecked(check != 0);
    }
    void SetRadio(BOOL on) override { SetCheck(on); }
    void SetText(LPCTSTR text) override { CCmdUI::SetText(text); }
};

// ---------------------------------------------------------------------------
// RmtQtBridge - IRmtHost for the tracker code, owner of its objects
// ---------------------------------------------------------------------------

// An accelerator of Rmt.rc as a Qt key sequence; empty for the keys the view handles itself (the numeric keypad's)
static Qt::KeyboardModifiers AcceleratorModifiers(const TRmtAccelerator& a)
{
    Qt::KeyboardModifiers modifiers;
    if (a.modifiers & RMT_ACC_CONTROL) modifiers |= Qt::ControlModifier;
    if (a.modifiers & RMT_ACC_SHIFT) modifiers |= Qt::ShiftModifier;
    if (a.modifiers & RMT_ACC_ALT) modifiers |= Qt::AltModifier;
    return modifiers;
}

static QKeySequence AcceleratorToQt(const TRmtAccelerator& a)
{
    int key = 0;
    if (a.vk >= 'A' && a.vk <= 'Z') {
        key = Qt::Key_A + (int)(a.vk - 'A');
    } else if (a.vk >= '0' && a.vk <= '9') {
        key = Qt::Key_0 + (int)(a.vk - '0');
    } else if (a.vk >= VK_F1 && a.vk <= VK_F12) {
        key = Qt::Key_F1 + (int)(a.vk - VK_F1);
    } else {
        switch (a.vk) {
            case VK_ESCAPE: key = Qt::Key_Escape; break;
            case VK_SPACE: key = Qt::Key_Space; break;
            case VK_RETURN: key = Qt::Key_Return; break;
            case VK_TAB: key = Qt::Key_Tab; break;
            case VK_DELETE: key = Qt::Key_Delete; break;
            case VK_INSERT: key = Qt::Key_Insert; break;
            default: return QKeySequence(); // VK_ADD, VK_SUBTRACT, ...: the numeric keypad is OnKeyDown's
        }
    }
    return QKeySequence(key | int(AcceleratorModifiers(a)));
}

// The text of Help > About: the version, then the people of RMT and of RITMO
static QString AboutText(const QString& version)
{
    return "<p><b>RITMO</b> - Atari POKEY music tracker<br>"
           "<i>RITMO Is a Tracker for Music On Atari</i></p>"
           "<p>" + version.toHtmlEscaped() + "<br>Qt frontend (Linux, Windows, macOS)</p>"
           "<p>Based on RASTER Music Tracker, by:<br>"
           "&nbsp;&nbsp;Radek &Scaron;t&#283;rba (Raster/C.P.U.), 2002-2009 - <b>R.I.P.</b><br>"
           "&nbsp;&nbsp;Vin Samuel (VinsCool), 2021-2024<br>"
           "&nbsp;&nbsp;Peter Dell (JAC!), 2024 to present</p>"
           "<p>With the work of:<br>"
           "&nbsp;&nbsp;Robert Petruzela (Bob!k/C.P.U.), JirkaS/C.P.U.<br>"
           "&nbsp;&nbsp;Gianluca Renzi: the Qt port<br>"
           "&nbsp;&nbsp;DMSC (LZSS), Rensoupp (unrolled LZSS driver), PG (graphics, ideas, beta testing)<br>"
           "&nbsp;&nbsp;synthpopalooza and OPNA2608 (POKEY tuning), Enderdude, Spring, Ivop, Tatqoo, Miker</p>"
           "<p>Thanks to: Fox/Taquart (XASM, ASAP), Jaskier/Taquart (TMC, RMT routine optimizations), "
           "Sack/Cosine, X-ray, Greg and Bewu/Grayscale, Fandal, ZdenekB, KrupkaJ, Pepax, LiSU, Dely, Nils Feske, "
           "Elan, Wrathchild, Kozyca, Born/LaResistance, Sal Esquivel, Nooly, "
           "The Chiptune Caf&eacute;, AtariAge, GBAtemp, the Polish Atarians of Atariarea "
           "and all the 8-bit Atarians all over the world!</p>";
}

class RmtQtBridge : public IRmtHost {
public:
    RmtQtBridge(RmtMainWindow* win) : m_win(win) {}

    RmtMainWindow* m_win;
    RmtViewWidget* m_widget = nullptr;
    QtRmtDoc m_doc;
    QtMainFrame m_frame;
    QtRmtView m_view;
    CBitmap m_windowBitmap; // 1x1, only for GetDC() (CreateCompatibleDC/Bitmap)
    CDC m_windowDC;
    QSize m_widgetSize; // a change forces a redraw (the view resizes its bitmap)
    std::map<UINT_PTR, QTimer*> m_timers;
    std::set<unsigned> m_keysDown;
    // All (id → action) pairs registered in the menu bar, for update-UI polling
    std::vector<std::pair<UINT, QAction*>> m_menuActions;
    std::vector<std::pair<UINT, QAction*>> m_keyActions; // the keys of Rmt.rc that have no menu item
    bool m_started = false;
    // The toolbars of CMainFrame::OnCreate() (m_wndToolBar, m_ToolBarBlock)
    QToolBar* m_mainToolBar = nullptr;
    QToolBar* m_blockToolBar = nullptr;
    QComboBox* m_linesAfter = nullptr; // m_comboSkipLinesAfterNoteInsert
    struct ToolAction {
        UINT id;
        QAction* action;
        QToolBar* bar;
    };
    std::vector<ToolAction> m_toolActions;
    int m_toolScaling = 0; // g_scaling_percentage of the icon size

    void Attach(RmtViewWidget* widget)
    {
        m_widget = widget;
        m_frame.m_hWnd = (HWND)m_win;
        m_view.m_hWnd = (HWND)widget;
        m_view.m_pDocument = &m_doc;
    }

    void EnsureWindowDC()
    {
        if (!m_windowBitmap.Width()) {
            m_windowBitmap.Create(1, 1);
            m_windowDC.CreateCompatibleDC(nullptr);
            m_windowDC.SelectObject(&m_windowBitmap);
        }
        QSize size(std::max(1, m_widget->width()), std::max(1, m_widget->height()));
        if (size != m_widgetSize) {
            m_widgetSize = size;
            SCREENUPDATE;
        }
    }

    // CRmtView::OnDraw() without its final StretchBlt: the screen into the
    // view's own bitmap (m_mem_dc), which holds the g_width x g_height screen,
    // when the tracker has flagged it. Returns the part of the widget that
    // changed since the last screen it shows.
    QRegion Draw()
    {
        EnsureWindowDC();
        if (!m_started || !g_screenupdate) {
            NO_SCREENUPDATE;
            return {};
        }
        if (g_view.debugDisplay) m_view.GetFPS();
        m_view.Resize();
        g_Song.RespectBoundaries();
        m_view.DrawAll();
        NO_SCREENUPDATE;
        return ChangedRegion();
    }

    // The screen the widget shows (g_width x g_height), to repaint only what a redraw changes: the timer redraws
    // the whole screen 60 times a second, and sending the whole window to the display each time costs more
    // than drawing it (most of all in the X server, on a slow machine)
    std::vector<uint32_t> m_shown;
    int m_shownWidth = 0, m_shownHeight = 0;

    QRegion ChangedRegion()
    {
        const CBitmap& bmp = m_view.m_mem_bitmap;
        if (!bmp.Bits()) return {};
        const int w = std::min(g_width, bmp.Width()), h = std::min(g_height, bmp.Height());
        const int stride = bmp.Width();
        if (w != m_shownWidth || h != m_shownHeight) {
            m_shownWidth = w;
            m_shownHeight = h;
            m_shown.resize((size_t)w * h);
            for (int y = 0; y < h; y++)
                std::memcpy(m_shown.data() + (size_t)y * w, bmp.Bits() + (size_t)y * stride, (size_t)w * 4);
            return QRegion(m_widget->rect());
        }
        // the rows that changed one after another make one rectangle, from the leftmost to the rightmost change
        QRegion region;
        int top = -1, left = w, right = 0;
        for (int y = 0; y <= h; y++) {
            const uint32_t* row = y < h ? bmp.Bits() + (size_t)y * stride : nullptr;
            uint32_t* shown = y < h ? m_shown.data() + (size_t)y * w : nullptr;
            if (row && std::memcmp(row, shown, (size_t)w * 4)) {
                int l = (int)(std::mismatch(row, row + w, shown).first - row);
                int r = w;
                while (row[r - 1] == shown[r - 1]) r--;
                std::memcpy(shown + l, row + l, (size_t)(r - l) * 4);
                if (top < 0) top = y;
                left = std::min(left, l);
                right = std::max(right, r);
            } else if (top >= 0) {
                region += ToWidget(QRect(left, top, right - left, y - top));
                top = -1;
                left = w;
                right = 0;
            }
        }
        return region;
    }

    // A rectangle of the screen in the widget, which shows it scaled (g_scaling_percentage)
    QRect ToWidget(const QRect& r) const
    {
        if (g_width == m_view.m_width && g_height == m_view.m_height) return r;
        const int x0 = r.left() * m_view.m_width / g_width, y0 = r.top() * m_view.m_height / g_height;
        const int x1 = ((r.right() + 1) * m_view.m_width + g_width - 1) / g_width;
        const int y1 = ((r.bottom() + 1) * m_view.m_height + g_height - 1) / g_height;
        return QRect(x0, y0, x1 - x0, y1 - y0).adjusted(-1, -1, 1, 1); // the rounding of the nearest neighbour
    }

    // Stopped, silent and with no input for a second: the screen is drawn 10 times a second instead of 60 (a
    // slow machine spends most of a core on the redraws, which change nothing then)
    std::chrono::steady_clock::time_point m_lastInput;
    unsigned m_idleTicks = 0;

    void NoteInput() { m_lastInput = std::chrono::steady_clock::now(); }

    bool Idle() const
    {
        if (!m_started || g_Song.GetPlayMode() != PLAY_STOP) return false;
        if (std::chrono::steady_clock::now() - m_lastInput < std::chrono::seconds(1)) return false;
        // a note still sounding (a key, MIDI): the volume of AUDC1-4 of both POKEYs, as the analyzer shows them
        const byte* memory = g_AtariTrackerDriver->GetAtari()->GetConstMemoryAt(0);
        for (int reg : { 0xd201, 0xd203, 0xd205, 0xd207, 0xd211, 0xd213, 0xd215, 0xd217 })
            if (memory[reg] & 0x0f) return false;
        return true;
    }

    // Invalidate() of the view: RefreshScreen() flags the redraw (SCREENUPDATE) after calling it, so the screen
    // is drawn when the events are processed, and only the parts that changed are repainted
    bool m_drawPending = false;

    void ScheduleDraw()
    {
        if (Idle()) {
            if (m_idleTicks++ % 6) return; // g_screenupdate stays set: the next draw shows it all
        } else
            m_idleTicks = 0;
        if (m_drawPending) return;
        m_drawPending = true;
        QTimer::singleShot(0, m_widget, [this] {
            m_drawPending = false;
            EnsureWindowDC();
            if (!m_started || !g_screenupdate) {
                m_widget->update(); // nothing new to draw: show the last screen again
                return;
            }
            const QRegion changed = Draw();
            if (!changed.isEmpty()) m_widget->update(changed);
        });
    }

    void Paint(QPainter& painter, const QRect& rect)
    {
        // a redraw still flagged (a resize, the first screen) is done now; what it changes outside this paint
        // event is repainted by another one
        const QRegion changed = Draw() - rect;
        if (!changed.isEmpty()) m_widget->update(changed);
        // the widget shows the view's bitmap, scaled by QPainter (nearest neighbour, no smoothing)
        const CBitmap& bmp = m_view.m_mem_bitmap;
        if (!bmp.Bits()) {
            painter.fillRect(rect, Qt::black);
            return;
        }
        QImage image((const uchar*)bmp.Bits(), bmp.Width(), bmp.Height(), bmp.Width() * 4, QImage::Format_RGB32);
        if (g_width == m_view.m_width && g_height == m_view.m_height) {
            const QRect part = rect & image.rect();
            painter.drawImage(part.topLeft(), image, part);
        } else
            painter.drawImage(QRect(0, 0, m_view.m_width, m_view.m_height), image, QRect(0, 0, g_width, g_height));
    }

    // File / Print, Print Preview, Print Setup: the tracker screen (the bitmap the window shows) on a page, as the
    // view prints its OnDraw(). The page is landscape and the picture fills it keeping its proportions.
    std::unique_ptr<QPrinter> m_printer;

    QPrinter& Printer()
    {
        if (!m_printer) {
            m_printer = std::make_unique<QPrinter>(QPrinter::HighResolution);
            m_printer->setPageOrientation(QPageLayout::Landscape);
        }
        return *m_printer;
    }

    void PaintPage(QPrinter* printer)
    {
        const CBitmap& bmp = m_view.m_mem_bitmap;
        if (!bmp.Bits()) return;
        QImage image((const uchar*)bmp.Bits(), bmp.Width(), bmp.Height(), bmp.Width() * 4, QImage::Format_RGB32);
        image = image.copy(0, 0, std::min(g_width, bmp.Width()), std::min(g_height, bmp.Height())); // the part in use
        QPainter painter(printer);
        QRect page = painter.viewport();
        QSize size = image.size();
        size.scale(page.size(), Qt::KeepAspectRatio);
        painter.setViewport(page.x(), page.y(), size.width(), size.height());
        painter.setWindow(image.rect());
        painter.drawImage(0, 0, image);
    }

    void FilePrint(UINT id)
    {
        // test runs (RMT_QT_GRAB: no modal dialogs): RMT_QT_PRINT_PDF=<file> prints into that file, else nothing
        const QByteArray pdf = qgetenv("RMT_QT_PRINT_PDF");
        if (!pdf.isEmpty()) {
            QPrinter printer(QPrinter::HighResolution);
            printer.setPageOrientation(QPageLayout::Landscape);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(QString::fromLocal8Bit(pdf));
            PaintPage(&printer);
            return;
        }
        if (!qEnvironmentVariableIsEmpty("RMT_QT_GRAB")) return;
        if (id == ID_FILE_PRINT_SETUP) {
            QPageSetupDialog dialog(&Printer(), m_win);
            dialog.exec();
        } else if (id == ID_FILE_PRINT_PREVIEW) {
            QPrintPreviewDialog dialog(&Printer(), m_win);
            QObject::connect(&dialog, &QPrintPreviewDialog::paintRequested, m_win, [this](QPrinter* printer) { PaintPage(printer); });
            dialog.exec();
        } else {
            QPrintDialog dialog(&Printer(), m_win);
            if (dialog.exec() == QDialog::Accepted) PaintPage(&Printer());
        }
    }

    // WM_COMMAND: the view, then the frame (the command routing of the Windows-style classes)
    void Dispatch(UINT id)
    {
        NoteInput();
        if (id == ID_FILE_PRINT || id == ID_FILE_PRINT_PREVIEW || id == ID_FILE_PRINT_SETUP) { // (the view maps them to MFC's printing)
            FilePrint(id);
            return;
        }
        if (m_view.OnPokeyCommand(id) || m_view.OnCmdMsg(id) || m_frame.OnCmdMsg(id)) return;
        switch (id) {
            case ID_APP_ABOUT:
            case ID_HELP_ABOUT_APP:
                if (qEnvironmentVariableIsEmpty("RMT_QT_GRAB")) {
                    QMessageBox box(m_win);
                    box.setWindowTitle("About RITMO");
                    box.setTextFormat(Qt::RichText);
                    box.setText(AboutText(QString::fromLocal8Bit(g_app.GetVersionAndBuild().GetString())));
                    // the icon of the program, enlarged without smoothing (pixel art)
                    box.setIconPixmap(QPixmap(":/ritmo-icon.png").scaled(128, 128, Qt::KeepAspectRatio, Qt::FastTransformation));
                    box.exec();
                }
                break;
            case ID_APP_EXIT:
                m_win->close();
                break;
            case ID_TOOLS_OPEN_ASAP_FILE: // no function yet: the item is grey
                break;
            case ID_HELP_ONLINE_HELP:
            case ID_HELP_HELP_TOPICS:
                g_app.OpenOnlineHelp();
                break;
            default:
                qDebug("RITMO: command %u has no handler", id);
        }
    }

    // Build the full menu bar from the MFC .rc menu structure.
    // Each action dispatches via Dispatch(id); ON_UPDATE_COMMAND_UI is run
    // on aboutToShow so enabled/checked states are kept in sync.
    void BuildMenuBar(QMenuBar* bar)
    {
        // Connect ON_UPDATE_COMMAND_UI for a menu's *direct* children only.
        // Each submenu is responsible for its own items via its own aboutToShow.
        auto connectUpdate = [&](QMenu* menu) {
            QObject::connect(menu, &QMenu::aboutToShow, [this, menu] {
                if (!m_started) return;
                for (QAction* act : menu->actions()) {
                    UINT id = act->data().toUInt();
                    if (!id) continue;
                    QtCCmdUI ui(id, act);
                    if (!m_view.OnUpdatePokeyCommand(&ui) && !m_view.OnUpdateCmdUI(&ui)) m_frame.OnUpdateCmdUI(&ui);
                }
            });
        };

        // The menu bar of Rmt.rc (g_rmtMenu, generated at configure time): popups, items and separators by level.
        // The keys are the accelerator table of Rmt.rc (g_rmtAccelerators); an item the table does not know takes
        // the key of its label as the shortcut when Qt knows it as a key sequence, else the label shows the key
        // the program handles itself (the Pokey Explorer's, the keys of OnKeyDown). The shortcuts belong to the
        // window, so that the dialogs on top of it keep their own keys.
        static const std::set<UINT> keysOfTheView = { ID_PLAYSTOP, ID_PROVEMODE, ID_SONG_INCREASE_PATTERN_STEP_SIZE, ID_SONG_DECREASE_PATTERN_STEP_SIZE };
        static const std::set<UINT> toggles = {
            ID_VIEW_TOOLBAR, ID_VIEW_BLOCKTOOLBAR, ID_VIEW_STATUS_BAR, ID_VIEW_PLAYTIMECOUNTER, ID_VIEW_VOLUMEANALYZER,
            ID_VIEW_POKEYREGS, ID_VIEW_INSTRUMENTACTIVEHELP, ID_PROVEMODE, ID_PLAYFOLLOW,
            ID_CHANNELS_CHANNEL1, ID_CHANNELS_CHANNEL2, ID_CHANNELS_CHANNEL3, ID_CHANNELS_CHANNEL4,
            ID_CHANNELS_CHANNEL5, ID_CHANNELS_CHANNEL6, ID_CHANNELS_CHANNEL7, ID_CHANNELS_CHANNEL8
        };
        auto shortcutOf = [&](UINT id, const std::string& hint) -> QList<QKeySequence> {
            if (keysOfTheView.count(id) || CPokeyController::IsCommand(id)) return {};
            for (const TRmtAccelerator* a = g_rmtAccelerators; a->id; a++) {
                if (a->id != id) continue;
                QList<QKeySequence> sequences;
                if (QKeySequence sequence = AcceleratorToQt(*a); !sequence.isEmpty()) sequences << sequence;
                if (a->vk == VK_RETURN) { // Alt+Enter: the Enter of the numeric keypad is another key to Qt
                    sequences << QKeySequence(int(Qt::Key_Enter) | int(AcceleratorModifiers(*a)));
                }
                return sequences;
            }
            if (hint.empty()) return {};
            QKeySequence sequence(QString::fromLatin1(hint.c_str()), QKeySequence::PortableText);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const int combined = sequence.count() == 1 ? sequence[0].toCombined() : 0; // Qt6: QKeyCombination
#else
            const int combined = sequence.count() == 1 ? sequence[0] : 0;
#endif
            if (combined == 0 || combined == Qt::Key_unknown) return {};
            return { sequence };
        };
        std::vector<QMenu*> menus; // menus[n]: where the entries of level n + 1 go
        for (const TRmtMenuEntry* e = g_rmtMenu; e->kind != RmtMenuKind::End; e++) {
            if (e->kind == RmtMenuKind::Popup) {
                QMenu* menu = e->level == 0 ? bar->addMenu(QString::fromLatin1(e->text)) : menus[e->level - 1]->addMenu(QString::fromLatin1(e->text));
                menus.resize(e->level);
                menus.push_back(menu);
                continue;
            }
            QMenu* menu = menus[e->level - 1];
            if (e->kind == RmtMenuKind::Separator) {
                menu->addSeparator();
                continue;
            }
            std::string text = e->text;
            std::string hint;
            if (size_t tab = text.find('\t'); tab != std::string::npos) {
                hint = text.substr(tab + 1);
                text.resize(tab);
            }
            QAction* act = menu->addAction(QString::fromLatin1(text.c_str()));
            act->setShortcutContext(Qt::WindowShortcut);
            QList<QKeySequence> shortcuts = shortcutOf(e->id, hint);
            if (!shortcuts.isEmpty()) {
                act->setShortcuts(shortcuts);
            } else if (!hint.empty()) {
                act->setText(QString::fromLatin1((text + "\t" + hint).c_str())); // shown in the shortcut column
            }
            UINT id = e->id;
            QObject::connect(act, &QAction::triggered, [this, id] { Dispatch(id); });
            m_menuActions.emplace_back(id, act);
            act->setData(id);
            // Toggle item: checkable at construction so SetCheck() in aboutToShow does not emit QAction::changed
            // (which would repaint the open menu)
            if (toggles.count(id)) act->setCheckable(true);
        }

        // The keys of the table that have no menu item (Ctrl+F12, Shift+F6): actions of the window without a menu
        for (const TRmtAccelerator* a = g_rmtAccelerators; a->id; a++) {
            bool inMenu = false;
            for (const auto& entry : m_menuActions) inMenu |= entry.first == a->id;
            if (inMenu || keysOfTheView.count(a->id)) continue;
            QKeySequence sequence = AcceleratorToQt(*a);
            if (sequence.isEmpty()) continue;
            QAction* act = new QAction(m_win);
            act->setShortcut(sequence);
            act->setShortcutContext(Qt::WindowShortcut);
            UINT id = a->id;
            QObject::connect(act, &QAction::triggered, [this, id] { Dispatch(id); });
            m_win->addAction(act);
            m_keyActions.emplace_back(id, act);
        }

        // Wire up ON_UPDATE_COMMAND_UI: each menu updates only its own direct
        // children (submenus update their own items via their own aboutToShow).
        std::function<void(QMenu*)> wireUpdate = [&](QMenu* menu) {
            connectUpdate(menu);
            for (QAction* act : menu->actions())
                if (act->menu()) wireUpdate(act->menu());
        };
        for (QAction* act : bar->actions())
            if (act->menu()) wireUpdate(act->menu());
    }

    // The command table of the program (script command "dump actions"): the items of the menu bar with their
    // texts and shortcuts, the buttons of the toolbars with their tooltips
    std::string ActionTable(QMenuBar* bar, int& errorCount)
    {
        std::map<QAction*, UINT> idOfAction;
        for (const auto& entry : m_menuActions) {
            idOfAction[entry.second] = entry.first;
        }
        std::vector<TActionMenuItem> menuItems;
        std::function<void(QMenu*, std::vector<std::string>&)> walk = [&](QMenu* menu, std::vector<std::string>& path) {
            for (QAction* act : menu->actions()) {
                if (act->isSeparator()) continue;
                if (act->menu()) {
                    path.push_back(act->text().toStdString());
                    walk(act->menu(), path);
                    path.pop_back();
                    continue;
                }
                auto it = idOfAction.find(act);
                if (it == idOfAction.end()) continue;
                TActionMenuItem item;
                item.id = it->second;
                item.path = path;
                item.label = act->text().toStdString();
                item.key = act->shortcut().toString(QKeySequence::PortableText).toStdString();
                if (size_t enter = item.key.find("Return"); enter != std::string::npos) item.key.replace(enter, 6, "Enter"); // as the other programs name it
                menuItems.push_back(item);
            }
        };
        for (QAction* top : bar->actions()) {
            if (!top->menu()) continue;
            std::vector<std::string> path{ top->text().toStdString() };
            walk(top->menu(), path);
        }
        std::vector<TActionToolButton> buttons;
        for (const ToolAction& t : m_toolActions) {
            TActionToolButton button;
            button.id = t.id;
            button.bar = t.bar->windowTitle().toStdString();
            const std::string suffix = " toolbar"; // "Main toolbar" is the Main one
            if (button.bar.size() > suffix.size() && button.bar.compare(button.bar.size() - suffix.size(), suffix.size(), suffix) == 0) {
                button.bar.resize(button.bar.size() - suffix.size());
            }
            button.tip = t.action->toolTip().toStdString();
            button.status = t.action->statusTip().toStdString();
            buttons.push_back(button);
        }
        std::vector<TActionKey> keys;
        for (const auto& entry : m_keyActions) {
            TActionKey key;
            key.id = entry.first;
            key.key = entry.second->shortcut().toString(QKeySequence::PortableText).toStdString();
            keys.push_back(key);
        }
        return BuildActionTable(menuItems, buttons, keys, errorCount);
    }

    // The toolbars of the .rc (IDR_MAINFRAME, IDR_TOOLBARBLOCK): 16x15 images
    // of their bitmap, one per button, the light gray of the bitmap is the
    // background; the tooltips and the status bar texts are the ones of the
    // string table
    void BuildToolBars()
    {
        struct Button {
            UINT id;
            const char* status;
            const char* tip;
        };
        auto build = [&](const char* title, UINT bitmap, std::initializer_list<Button> buttons) {
            auto* bar = new QToolBar(title, m_win);
            bar->setObjectName(title);
            bar->setFloatable(false);
            bar->setContextMenuPolicy(Qt::PreventContextMenu);
            QImage image;
            const unsigned char* data;
            size_t size;
            if (RmtFindResource(bitmap, &data, &size)) image = QImage::fromData(data, (int)size, "BMP");
            image = image.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < image.height(); y++) {
                QRgb* line = (QRgb*)image.scanLine(y);
                for (int x = 0; x < image.width(); x++)
                    if ((line[x] & 0xFFFFFF) == 0xC0C0C0) line[x] = 0;
            }
            int index = 0;
            for (const Button& b : buttons) {
                if (!b.id) { // SEPARATOR
                    bar->addSeparator();
                    continue;
                }
                QImage tile = image.copy(index++ * 16, 0, 16, 15);
                QIcon icon;
                for (int scale = 1; scale <= 3; scale++) // nearest neighbour, like the view
                    icon.addPixmap(QPixmap::fromImage(tile.scaled(16 * scale, 15 * scale)));
                if (b.id == ID_BUTTONCOMBO1) { // the combo takes the place of this button
                    m_linesAfter = new QComboBox(bar);
                    for (int i = 0; i <= 8; i++) m_linesAfter->addItem(QString::number(i));
                    m_linesAfter->setCurrentIndex(g_linesafter);
                    m_linesAfter->setToolTip(b.tip);
                    m_linesAfter->setStatusTip(b.status);
                    m_linesAfter->setFocusPolicy(Qt::ClickFocus);
                    // OnSelChangedComboSkipLinesAfterNoteInsert(), OnRestoreFocusToMainWindow()
                    QObject::connect(m_linesAfter, QOverload<int>::of(&QComboBox::activated), m_win, [this](int i) {
                        g_linesafter = i;
                        m_widget->setFocus();
                    });
                    bar->addWidget(m_linesAfter);
                    continue;
                }
                QAction* act = bar->addAction(icon, b.tip);
                act->setToolTip(b.tip);
                act->setStatusTip(b.status);
                UINT id = b.id;
                QObject::connect(act, &QAction::triggered, m_win, [this, id] {
                    Dispatch(id);
                    UpdateToolBars();
                    m_widget->setFocus();
                });
                m_toolActions.push_back({ id, act, bar });
            }
            m_win->addToolBar(Qt::TopToolBarArea, bar);
            return bar;
        };
        m_mainToolBar = build("Main toolbar", IDR_MAINFRAME, {
                                                                 { ID_FILE_NEW, "Create a new song", "New" },
                                                                 { ID_FILE_OPEN, "Load an existing song", "Load" },
                                                                 { ID_FILE_SAVE, "Save the song", "Save" },
                                                                 { ID_FILE_EXPORT_AS, "Export song to file", "Export" },
                                                                 { 0, nullptr, nullptr },
                                                                 { ID_APP_ABOUT, "Display program information, version number and copyright", "About" },
                                                                 { 0, nullptr, nullptr },
                                                                 { ID_PLAY0, "Play song from bookmark position", "Play from bookmark" },
                                                                 { ID_PLAY1, "Play song from start position", "Play from start" },
                                                                 { ID_PLAY2, "Play song from current position", "Play" },
                                                                 { ID_PLAY3, "Play and loop current tracks pattern", "Loop pattern" },
                                                                 { ID_PLAYSTOP, "Stop playing the song. Mute all sounds.", "Stop" },
                                                                 { 0, nullptr, nullptr },
                                                                 { ID_PLAYFOLLOW, "Follow the currently playing position (turn on/off)", "Follow song" },
                                                                 { 0, nullptr, nullptr },
                                                                 { ID_EM_TRACKS, "Move to track edit view", "Track edit" },
                                                                 { ID_EM_INSTRUMENTS, "Move to instrument edit view", "Instrument edit" },
                                                                 { ID_EM_INFO, "Move cursor to Info edit", "Info edit" },
                                                                 { ID_EM_SONG, "Move cursor to song edit", "Song edit" },
                                                                 { 0, nullptr, nullptr },
                                                                 { ID_PROVEMODE, "Edit/Jam mode toggle", "Jam mode" },
                                                                 { 0, nullptr, nullptr },
                                                                 { ID_MIDIONOFF, "MIDI on/off", "MIDI on/off" },
                                                                 { 0, nullptr, nullptr },
                                                                 { ID_BUTTONCOMBO1, "Insert note spacing", "Insert note spacing" },
                                                             });
        m_blockToolBar = build("Block toolbar", IDR_TOOLBARBLOCK, {
                                                                      { ID_BLOCK_BACKUP, "Restore block from backup", "Restore block" },
                                                                      { 0, nullptr, nullptr },
                                                                      { ID_BLOCK_NOTEUP, "Note transposition up", "Transpose up" },
                                                                      { ID_BLOCK_NOTEDOWN, "Note transposition down", "Transpose down" },
                                                                      { ID_BLOCK_INSTRLEFT, "Instrument number change", "Change instrument" },
                                                                      { ID_BLOCK_INSTRRIGHT, "Instrument number change", "Change instrument" },
                                                                      { ID_BLOCK_VOLUMEUP, "Volume up", "Volume up" },
                                                                      { ID_BLOCK_VOLUMEDOWN, "Volume down", "Volume down" },
                                                                      { ID_BLOCK_EFFECT, "Effects/tools", "Effects/tools" },
                                                                      { 0, nullptr, nullptr },
                                                                      { ID_BLOCK_INSTRALL, "Block modification mode", "Block mode" },
                                                                      { 0, nullptr, nullptr },
                                                                      { ID_BLOCK_PLAY, "Play selected block", "Play block" },
                                                                  });
        UpdateToolBars();
    }

    // ON_UPDATE_COMMAND_UI of the buttons (run when idle), the combo
    // follows g_linesafter (Ctrl+numpad +/-, new song), the icons the
    // interface size
    void UpdateToolBars()
    {
        if (!m_started || !m_mainToolBar) return;
        for (const ToolAction& t : m_toolActions) {
            if (t.bar->isHidden()) continue;
            QtToolCmdUI ui(t.id, t.action);
            if (!m_view.OnUpdateCmdUI(&ui)) m_frame.OnUpdateCmdUI(&ui);
        }
        if (m_linesAfter->currentIndex() != g_linesafter && g_linesafter >= 0 && g_linesafter <= 8)
            m_linesAfter->setCurrentIndex(g_linesafter);
        if (m_toolScaling != g_scaling_percentage) {
            m_toolScaling = g_scaling_percentage;
            QSize size(16 * m_toolScaling / 100, 15 * m_toolScaling / 100);
            m_mainToolBar->setIconSize(size);
            m_blockToolBar->setIconSize(size);
        }
    }

    QWidget* ControlBarWidget(const CControlBar* bar)
    {
        if (bar == &m_frame.m_wndToolBar) return m_mainToolBar;
        if (bar == &m_frame.m_ToolBarBlock) return m_blockToolBar;
        if (bar == (const CControlBar*)&m_frame.m_wndStatusBar) return m_win->statusBar();
        return nullptr;
    }

    static UINT MouseFlags(Qt::MouseButtons b, Qt::KeyboardModifiers m)
    {
        UINT f = 0;
        if (b & Qt::LeftButton) f |= MK_LBUTTON;
        if (b & Qt::RightButton) f |= MK_RBUTTON;
        if (b & Qt::MiddleButton) f |= MK_MBUTTON;
        if (m & Qt::ShiftModifier) f |= MK_SHIFT;
        if (m & Qt::ControlModifier) f |= MK_CONTROL;
        return f;
    }

    // --- IRmtHost ---

    CFrameWnd* GetMainWnd() override { return &m_frame; }

    void GetClientRect(const CWnd* wnd, RECT* r) override
    {
        QWidget* w = wnd == &m_frame ? (QWidget*)m_win : (QWidget*)m_widget;
        *r = RECT{ 0, 0, w->width(), w->height() };
    }

    void Invalidate(CWnd*) override { ScheduleDraw(); }

    CDC* GetDC(CWnd*) override
    {
        EnsureWindowDC();
        return &m_windowDC;
    }

    UINT_PTR SetTimer(CWnd*, UINT_PTR id, UINT ms) override
    {
        QTimer*& t = m_timers[id];
        if (!t) {
            t = new QTimer(m_widget);
            QObject::connect(t, &QTimer::timeout, [this, id] { m_view.OnTimer(id); });
        }
        t->start(ms);
        return id;
    }

    BOOL KillTimer(CWnd*, UINT_PTR id) override
    {
        auto it = m_timers.find(id);
        if (it == m_timers.end() || !it->second) return FALSE;
        it->second->stop();
        return TRUE;
    }

    void PostCommand(UINT id) override
    {
        QTimer::singleShot(0, m_widget, [this, id] { Dispatch(id); });
    }

    void Close() override
    {
        QTimer::singleShot(0, m_win, [this] { m_win->close(); });
    }

    int MessageBox(const char* text, const char* caption, UINT type) override
    {
        int scriptAnswer = IDOK;
        if (ScriptMessageBox(text, caption, type, scriptAnswer)) { // rmt /SCRIPT: the console takes the box's place
            return scriptAnswer;
        }
        if (!qEnvironmentVariableIsEmpty("RMT_QT_GRAB")) { // test run: no modal dialogs
            qWarning("[MessageBox] %s: %s", caption ? caption : "", text ? text : "");
            return (type & 0x0F) == MB_YESNO || (type & 0x0F) == MB_YESNOCANCEL ? IDNO : IDOK;
        }
        QMessageBox box(m_win);
        box.setWindowTitle(caption ? caption : "RITMO");
        box.setText(text ? text : "");
        switch (type & 0xF0) {
            case MB_ICONERROR: box.setIcon(QMessageBox::Critical); break;
            case MB_ICONWARNING: box.setIcon(QMessageBox::Warning); break;
            case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
            case MB_ICONINFORMATION: box.setIcon(QMessageBox::Information); break;
        }
        switch (type & 0x0F) {
            case MB_OKCANCEL: box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel); break;
            case MB_YESNO: box.setStandardButtons(QMessageBox::Yes | QMessageBox::No); break;
            case MB_YESNOCANCEL: box.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel); break;
            default: box.setStandardButtons(QMessageBox::Ok);
        }
        switch (box.exec()) {
            case QMessageBox::Yes: return IDYES;
            case QMessageBox::No: return IDNO;
            case QMessageBox::Cancel: return IDCANCEL;
            default: return IDOK;
        }
    }

    HCURSOR LoadCursor(UINT id) override { return (HCURSOR)(uintptr_t)id; }

    void SetCursor(HCURSOR cursor) override
    {
        switch ((UINT)(uintptr_t)cursor) {
            case IDC_CURSORGOTO:
            case IDC_CURSORDLG:
            case IDC_CURSORCHANNELONOFF: m_widget->setCursor(Qt::PointingHandCursor); break;
            case IDC_CURSORENVVOLUME: m_widget->setCursor(Qt::SizeVerCursor); break;
            case IDC_CURSORSETPOS: m_widget->setCursor(Qt::CrossCursor); break;
            case 32514: m_widget->setCursor(Qt::WaitCursor); break; // IDC_WAIT
            default: m_widget->setCursor(Qt::ArrowCursor);
        }
    }

    short GetKeyState(int vk) override
    {
        Qt::KeyboardModifiers m = QGuiApplication::queryKeyboardModifiers();
        bool down;
        switch (vk) {
            case VK_SHIFT:
            case VK_LSHIFT:
            case VK_RSHIFT: down = m & Qt::ShiftModifier; break;
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL: down = m & Qt::ControlModifier; break;
            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU: down = m & Qt::AltModifier; break;
            case VK_CAPITAL: return 0; // toggle state unknown
            default: down = m_keysDown.count(vk) > 0;
        }
        return down ? (short)0x8000 : 0;
    }

    UINT MapVirtualKeyToChar(UINT vk) override { return RmtVirtualKeyToChar(vk); }

    void SetWindowText(CWnd* wnd, const char* text) override
    {
        if (wnd == &m_frame)
            m_win->setWindowTitle(text);
        else if (wnd == &m_frame.m_wndStatusBar)
            m_win->statusBar()->showMessage(text);
    }

    void ShowControlBar(CControlBar* bar, BOOL show) override
    {
        if (QWidget* w = ControlBarWidget(bar)) w->setVisible(show);
    }
    BOOL IsControlBarVisible(const CControlBar* bar) override
    {
        QWidget* w = ControlBarWidget(bar);
        return w && !w->isHidden();
    }
    void SetStatusText(int, const char* text) override { m_win->statusBar()->showMessage(text); }

    bool FileDialog(bool open, const char* title, const char* initialDir, const char* fileName,
                    const char* filter, DWORD flags, int& filterIndex, CString& path) override
    {
        // MFC filter "Name (*.a)|*.a;*.b|...||" -> Qt name filter "Name (*.a *.A *.b *.B)":
        // the patterns come from the second field, in both cases (Linux file
        // names are case sensitive, Atari files are often upper case)
        QStringList filters, suffixes;
        QList<QStringList> patternLists;
        QStringList parts = QString::fromLocal8Bit(filter ? filter : "").split('|');
        for (int i = 0; i + 1 < parts.size() && !parts[i].isEmpty(); i += 2) {
            QString name = parts[i];
            int paren = name.lastIndexOf('(');
            if (paren > 0) name = name.left(paren).trimmed();
            QStringList patterns;
            for (const QString& p : parts[i + 1].split(';', Qt::SkipEmptyParts)) {
                patterns << p.trimmed().toLower();
                if (p.trimmed().toUpper() != patterns.last()) patterns << p.trimmed().toUpper();
            }
            filters << QString("%1 (%2)").arg(name, patterns.join(' '));
            suffixes << (patterns.value(0).startsWith("*.") ? patterns.value(0).mid(2) : QString());
            patternLists << patterns;
        }
        int index = filterIndex >= 1 && filterIndex <= filters.size() ? filterIndex : 1;

        if (!qEnvironmentVariableIsEmpty("RMT_QT_GRAB")) { // test run: no modal dialogs
            // RMT_QT_FILEDIALOG="a.rmt,b.txt" answers the file dialogs in turn,
            // with the filter that matches the file, or the one given as
            // "file@N" (1-based); none left: cancel
            static QStringList answers = qEnvironmentVariable("RMT_QT_FILEDIALOG").split(',', Qt::SkipEmptyParts);
            if (answers.isEmpty()) {
                qWarning("[FileDialog] %s: cancelled", title ? title : "");
                return false;
            }
            QString answer = answers.takeFirst();
            int at = answer.lastIndexOf('@');
            bool forced = false;
            if (at > 0) {
                int n = answer.mid(at + 1).toInt(&forced);
                if (forced && n >= 1 && n <= filters.size()) {
                    index = n;
                    answer = answer.left(at);
                } else
                    forced = false;
            }
            for (int i = 0; !forced && i < patternLists.size(); i++)
                if (QDir::match(patternLists[i].join(' '), QFileInfo(answer).fileName())) {
                    index = i + 1;
                    break;
                }
            qWarning("[FileDialog] %s: %s (filter %d)", title ? title : "", qPrintable(answer), index);
            path = answer.toLocal8Bit().constData();
            filterIndex = index;
            return true;
        }

        QFileDialog dlg(m_win, title ? QString::fromLocal8Bit(title) : QString());
        dlg.setAcceptMode(open ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
        dlg.setFileMode(open ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
        if (!open && !(flags & OFN_OVERWRITEPROMPT)) dlg.setOption(QFileDialog::DontConfirmOverwrite);
        if (initialDir && *initialDir) dlg.setDirectory(QString::fromLocal8Bit(initialDir));
        if (!filters.isEmpty()) {
            dlg.setNameFilters(filters);
            dlg.selectNameFilter(filters[index - 1]);
        }
        // A save without extension gets the one of the chosen filter, so the
        // overwrite prompt checks the file that is really written
        auto setSuffix = [&](int i) { if (!open && i >= 0 && i < suffixes.size()) dlg.setDefaultSuffix(suffixes[i]); };
        setSuffix(index - 1);
        QObject::connect(&dlg, &QFileDialog::filterSelected, [&](const QString& f) { setSuffix(filters.indexOf(f)); });
        if (fileName && *fileName) dlg.selectFile(QString::fromLocal8Bit(fileName));

        bool ok = dlg.exec() == QDialog::Accepted && !dlg.selectedFiles().isEmpty();
        m_widget->setFocus();
        if (!ok) return false;
        path = dlg.selectedFiles().first().toLocal8Bit().constData();
        int selected = filters.indexOf(dlg.selectedNameFilter());
        filterIndex = selected >= 0 ? selected + 1 : index;
        return true;
    }

    INT_PTR DoModal(CDialog* dlg) override
    {
        INT_PTR result = RmtQtRunDialog(m_win, dlg);
        m_widget->setFocus();
        return result;
    }
};

// CRmtApp::OpenUrl() (the application object, CompatTypes.h); the test runs (RMT_QT_GRAB) open no browser
void CRmtApp::OpenUrl(const char* url)
{
    if (qEnvironmentVariableIsEmpty("RMT_QT_GRAB")) {
        QDesktopServices::openUrl(QUrl(QString::fromUtf8(url)));
    }
}

// CRmtApp::OpenOnlineHelp()
void CRmtApp::OpenOnlineHelp()
{
    QDesktopServices::openUrl(QUrl("https://html-preview.github.io/?url=https://github.com/raster-atari-org/RASTER-Music-Tracker/blob/1.35/doc//rmt_en.html"));
}

// ---------------------------------------------------------------------------
// RmtViewWidget
// ---------------------------------------------------------------------------

RmtViewWidget::RmtViewWidget(RmtQtBridge* bridge, QWidget* parent) : QWidget(parent), m_bridge(bridge)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(640, 400);
}

bool RmtViewWidget::event(QEvent* e)
{
    switch (e->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::Wheel:
        m_bridge->NoteInput(); // the screen at full rate (RmtQtBridge::Idle)
        break;
    default:
        break;
    }
    return QWidget::event(e);
}

void RmtViewWidget::paintEvent(QPaintEvent* e)
{
    QPainter painter(this);
    m_bridge->Paint(painter, e->rect());
}

// nFlags of WM_KEYDOWN / WM_KEYUP: scan code, bit 14 = key was already down
static UINT KeyFlags(QKeyEvent* e, bool wasDown)
{
    return (e->nativeScanCode() & 0xFF) | (wasDown ? 0x4000 : 0);
}

void RmtViewWidget::keyPressEvent(QKeyEvent* e)
{
    unsigned vk = RmtVirtualKey(e);
    if (!vk || !m_bridge->m_started) return;
    bool wasDown = m_bridge->m_keysDown.count(vk) > 0;
    m_bridge->m_keysDown.insert(vk);
    // Alt+key is WM_SYSKEYDOWN on Windows: not an OnKeyDown of the view
    if ((e->modifiers() & Qt::AltModifier) && !(e->modifiers() & Qt::ControlModifier)) {
        if (!e->text().isEmpty()) m_bridge->m_view.OnSysChar(e->text().at(0).unicode(), 1, KeyFlags(e, wasDown));
        return;
    }
    m_bridge->m_view.OnKeyDown(vk, 1, KeyFlags(e, wasDown));
}

void RmtViewWidget::keyReleaseEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) return;
    unsigned vk = RmtVirtualKey(e);
    if (!vk || !m_bridge->m_started) return;
    m_bridge->m_keysDown.erase(vk);
    m_bridge->m_view.OnKeyUp(vk, 1, KeyFlags(e, true) | 0x8000);
}

void RmtViewWidget::mousePressEvent(QMouseEvent* e)
{
    if (!m_bridge->m_started) return;
    UINT f = RmtQtBridge::MouseFlags(e->buttons(), e->modifiers());
    CPoint p(e->pos().x(), e->pos().y());
    if (e->button() == Qt::LeftButton)
        m_bridge->m_view.OnLButtonDown(f, p);
    else if (e->button() == Qt::RightButton)
        m_bridge->m_view.OnRButtonDown(f, p);
}

void RmtViewWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (!m_bridge->m_started) return;
    UINT f = RmtQtBridge::MouseFlags(e->buttons(), e->modifiers());
    CPoint p(e->pos().x(), e->pos().y());
    if (e->button() == Qt::LeftButton)
        m_bridge->m_view.OnLButtonUp(f, p);
    else if (e->button() == Qt::RightButton)
        m_bridge->m_view.OnRButtonUp(f, p);
}

void RmtViewWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (!m_bridge->m_started) return;
    UINT f = RmtQtBridge::MouseFlags(e->buttons(), e->modifiers());
    CPoint p(e->pos().x(), e->pos().y());
    if (e->button() == Qt::LeftButton)
        m_bridge->m_view.OnLButtonDblClk(f, p);
    else if (e->button() == Qt::RightButton)
        m_bridge->m_view.OnRButtonDblClk(f, p);
}

void RmtViewWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_bridge->m_started) return;
    m_bridge->m_view.OnMouseMove(RmtQtBridge::MouseFlags(e->buttons(), e->modifiers()), CPoint(e->pos().x(), e->pos().y()));
}

void RmtViewWidget::wheelEvent(QWheelEvent* e)
{
    if (!m_bridge->m_started) return;
    // CRmtView::OnMouseWheel() subtracts the window origin (0,0 here): client coordinates
    QPoint p = e->position().toPoint();
    m_bridge->m_view.OnMouseWheel(RmtQtBridge::MouseFlags(e->buttons(), e->modifiers()),
                                  (short)e->angleDelta().y(), CPoint(p.x(), p.y()));
}

void RmtViewWidget::focusInEvent(QFocusEvent*)
{
    if (m_bridge->m_started) m_bridge->m_view.OnSetFocus(nullptr);
}

void RmtViewWidget::focusOutEvent(QFocusEvent*)
{
    m_bridge->m_keysDown.clear();
    if (m_bridge->m_started) m_bridge->m_view.OnKillFocus(nullptr);
}

// ---------------------------------------------------------------------------
// RmtMainWindow
// ---------------------------------------------------------------------------

RmtMainWindow::RmtMainWindow() : m_bridge(new RmtQtBridge(this))
{
    auto view = new RmtViewWidget(m_bridge.get(), this);
    setCentralWidget(view);
    m_bridge->Attach(view);
    g_rmtHost = m_bridge.get();
    statusBar();
    m_bridge->BuildMenuBar(menuBar());
    m_bridge->BuildToolBars();
    auto* toolUpdate = new QTimer(this);
    QObject::connect(toolUpdate, &QTimer::timeout, this, [this] { m_bridge->UpdateToolBars(); });
    toolUpdate->start(100);
    setWindowTitle("RITMO");
}

RmtMainWindow::~RmtMainWindow()
{
    for (auto& t : m_bridge->m_timers)
        if (t.second) t.second->stop();
    if (m_bridge->m_started) m_bridge->m_view.OnDestroy();
    g_rmtHost = nullptr;
}

void RmtMainWindow::OverrideScaling(int percent)
{
    if (percent < 100 || percent > 300 || percent == g_scaling_percentage) return;
    if (!g_scalingToKeep) g_scalingToKeep = g_scaling_percentage;
    g_scaling_percentage = percent;
    m_bridge->m_view.m_width = m_bridge->m_view.m_height = 0;
    m_bridge->m_view.Resize(); // scales everything without resizing the window first
    SCREENUPDATE;
    centralWidget()->update();
}

void RmtMainWindow::Start(const QString& songFile)
{
    g_statusBar = &m_bridge->m_frame.m_wndStatusBar;
    m_bridge->m_view.OnInitialUpdate();
    m_bridge->m_started = true;
    if (!songFile.isEmpty()) g_Song.FileOpen(songFile.toLocal8Bit().constData(), FALSE);
    centralWidget()->setFocus();
    SCREENUPDATE;
    centralWidget()->update();
}

void RmtMainWindow::closeEvent(QCloseEvent* e)
{
    // CMainFrame::OnClose(): quit only once CRmtView::OnWantExit() said so
    // (unsaved changes, configuration saved), else ask it (File/Exit)
    if (g_closeApplication || !m_bridge->m_started) {
        if (m_bridge->m_started) RmtSaveWindowGeometry(saveGeometry()); // size and position for the next start
        e->accept();
        return;
    }
    e->ignore();
    m_bridge->Dispatch(ID_FILE_EXIT);
}

// The command table of the running program as Markdown, for the script command "dump actions" (ScriptRunner.cpp)
std::string RmtActionTable(int& errorCount)
{
    auto* bridge = static_cast<RmtQtBridge*>(g_rmtHost);
    return bridge->ActionTable(bridge->m_win->menuBar(), errorCount);
}
