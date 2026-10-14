// main-qt.cpp - RITMO, Qt frontend
//
// The start-up of the program, then the main window. The first command line
// argument is a song to open.

#include "StdAfx.h"
#include "Global.h"
#include "RmtQtScreen.h"
#include "RmtQtSettings.h"
#include "Song.h"
#include "Atari.h"
#include "AtariTrackerDriver.h"
#include "Tuning.h"
#include "RmtMidi.h"
#include "resource.h"

#include "RmtQtFrontend.h"
#include "ScriptRunner.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QTimer>

#include <filesystem>

extern CSong g_Song;
extern CAtari g_Atari;
extern TTuningSettings g_tuning;
extern TTuningRatios g_tuningRatios;
extern CRmtMidi g_Midi;

// rmt /SCRIPT:<file> (also -script:<file> and --script=<file>): runs an RMT script (doc/rmt_scripting.md) and exits
// with its code instead of showing the window. Empty when the command line has none.
static QString ScriptFileFromCommandLine(int argc, char** argv)
{
    static const char* prefixes[] = { "/script:", "-script:", "--script=" };
    for (int i = 1; i < argc; i++) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        for (const char* prefix : prefixes) {
            if (arg.startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) {
                return arg.mid((int)strlen(prefix));
            }
        }
    }
    return QString();
}

// ritmo --scale=<100..300> (also /scale:<n> and -scale:<n>): the interface size of this session, in percent, whatever the
// settings say and without changing them. 0 when the command line has none.
static int ScaleFromCommandLine(int argc, char** argv)
{
    static const char* prefixes[] = { "/scale:", "-scale:", "--scale=" };
    for (int i = 1; i < argc; i++) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        for (const char* prefix : prefixes) {
            if (arg.startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) {
                bool ok = false;
                const int percent = arg.mid((int)strlen(prefix)).toInt(&ok);
                return ok ? std::clamp(percent, 100, 300) : 0;
            }
        }
    }
    return 0;
}

// The song of the command line: the first argument that is not one of the options above
static QString SongFromCommandLine(int argc, char** argv)
{
    static const char* options[] = { "/script:", "-script:", "--script=", "/scale:", "-scale:", "--scale=" };
    for (int i = 1; i < argc; i++) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        bool isOption = false;
        for (const char* prefix : options) {
            if (arg.startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) isOption = true;
        }
        if (!isOption) return arg;
    }
    return QString();
}

// What the screen has for a window, in logical pixels. The platform without a screen (offscreen: the tests, the
// scripts, the screenshots) is not limited.
static bool ScreenArea(QRect& area)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen || QGuiApplication::platformName() == QLatin1String("offscreen")) return false;
    area = screen->availableGeometry();
    return true;
}

// The size and the position of the last session, within the screen that there is now
static bool RestoreWindow(RmtMainWindow& window)
{
    const QByteArray saved = RmtLoadWindowGeometry();
    if (saved.isEmpty() || !window.restoreGeometry(saved)) return false;
    QRect area;
    if (ScreenArea(area)) {
        int w = window.width(), h = window.height();
        RmtFitToScreen(w, h, area.width(), area.height());
        if (w != window.width() || h != window.height()) window.resize(w, h);
    }
    return true;
}

// The window of a first start: 1366x768, the screen of a small laptop, at the interface size (200 % on a screen that
// has the room for it), within what the screen has, in the middle of it
static void DefaultWindow(RmtMainWindow& window, int scalingPercent)
{
    int w = RMT_DEFAULT_WINDOW_WIDTH * scalingPercent / 100;
    int h = RMT_DEFAULT_WINDOW_HEIGHT * scalingPercent / 100;
    QRect area;
    const bool limited = ScreenArea(area);
    if (limited) RmtFitToScreen(w, h, area.width(), area.height());
    window.resize(w, h);
    if (limited) window.move(std::max(area.left(), area.center().x() - w / 2), std::max(area.top(), area.center().y() - (h + RMT_WINDOW_DECORATION_HEIGHT) / 2));
}

int main(int argc, char** argv)
{
    const QString scriptFile = ScriptFileFromCommandLine(argc, argv);
    const bool showScriptWindow = qEnvironmentVariableIntValue("RMT_SCRIPT_SHOW_WINDOW") != 0;
#ifdef Q_OS_LINUX
    // A script needs no display: without one chosen, Qt draws into memory
    if (!scriptFile.isEmpty() && !showScriptWindow && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
#endif
    // Qt scales by the fractional desktop DPI (Xft.dpi 106 -> 1.1), which
    // blurs the pixel-exact bitmaps; round it down to a whole factor (1.5 ->
    // 1, not 2: the window must still fit the screen).
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
    QApplication app(argc, argv);
    QApplication::setApplicationName("RITMO");
    // RMT_QT_FONT="DejaVu Sans,12.75": the font of the menus and of the
    // dialogs (as QFont::fromString reads it), for test runs and screenshots
    // that do not depend on the font of the desktop
    if (qEnvironmentVariableIsSet("RMT_QT_FONT")) {
        QFont font;
        if (font.fromString(qEnvironmentVariable("RMT_QT_FONT"))) QApplication::setFont(font);
    }
    QIcon icon; // the icon of the windows (and of the task bar), see res/ritmo-icon.png
    icon.addFile(":/ritmo-icon-16.png"); // drawn by hand, the title bar of the windows
    icon.addFile(":/ritmo-icon-32.png");
    icon.addFile(":/ritmo-icon.png");
    QApplication::setWindowIcon(icon);

    SetProgramFolderPath(CString((QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) + QDir::separator()).toLocal8Bit().constData()));

    // The 6502 and the POKEY are the built-in emulations (emu/): there are no libraries to load.
    g_Atari.Init();
    g_tuning.Initialize(g_Song.IsNTSC());
    g_tuningRatios.Initialize();
    g_Atari.Init(g_Song.IsNTSC());

    g_AtariTrackerDriver = new CAtariTrackerDriver(g_Atari);
    g_AtariTrackerDriver->LoadRMTRoutines(g_trackerDriverVersion);
    g_AtariTrackerDriver->Init();

    g_Song.ClearSong(8);

    RmtMainWindow window;
    const int scaleOverride = ScaleFromCommandLine(argc, argv);
    // the size of the last session, else the default one (set again below, once the interface size is known)
    const bool windowRestored = scriptFile.isEmpty() && RestoreWindow(window);
    if (!windowRestored) DefaultWindow(window, scaleOverride ? scaleOverride : 100);

    if (!scriptFile.isEmpty()) {
        // The window exists (the session needs it) but stays hidden, unless RMT_SCRIPT_SHOW_WINDOW=1 shows what the
        // script does. The configuration (ritmo.ini, tuning.ini) is read as for the window; it is not written back.
        if (showScriptWindow) {
            window.show();
        }
        g_rmtAudioOutput = false; // nothing is played, the exports render through the POKEY by themselves
        window.Start(QString());
        const CString scriptPath = CString(scriptFile.toLocal8Bit().constData());
        AttachScriptConsole(scriptPath);
        CScriptRunner runner(g_Song);
        const QByteArray outputOverride = qgetenv("RMT_SCRIPT_OUTPUT"); // runs one script into several folders
        if (!outputOverride.isEmpty()) {
            runner.SetOutputFolder(std::filesystem::path(outputOverride.constData()));
        }
        const int code = runner.RunFile(scriptPath);
        g_Song.StopTimer();
        g_Midi.MidiOff();
        return code;
    }

    window.show();
    // test hook: RMT_QT_GRAB=file.png saves the window after 1 s and quits
    // (with QT_QPA_PLATFORM=offscreen it runs without a display; the delay is
    // RMT_QT_GRAB_MS);
    // RMT_QT_KEYS="108,108,106" first presses those keys (Linux evdev codes);
    // RMT_QT_MENU_GRAB=prefix also saves the menus as images;
    // RMT_QT_COMMANDS="0xE101,0xE104" then triggers the menu actions of those
    // command IDs, or sends them as WM_COMMAND when they have no menu item
    // (file dialogs are answered by RMT_QT_FILEDIALOG)
    QString grab = qEnvironmentVariable("RMT_QT_GRAB");
    if (!grab.isEmpty()) {
        QString keys = qEnvironmentVariable("RMT_QT_KEYS");
        QString commands = qEnvironmentVariable("RMT_QT_COMMANDS");
        QTimer::singleShot(500, &window, [&window, keys, commands] {
            for (const QString& k : keys.split(',', Qt::SkipEmptyParts)) {
                quint32 scan = k.toUInt() + 8;
                QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, scan, 0, 0);
                QKeyEvent release(QEvent::KeyRelease, 0, Qt::NoModifier, scan, 0, 0);
                QCoreApplication::sendEvent(window.centralWidget(), &press);
                QCoreApplication::sendEvent(window.centralWidget(), &release);
            }
            std::function<QAction*(QMenu*, uint)> findAction = [&](QMenu* m, uint id) -> QAction* {
                for (QAction* a : m->actions()) {
                    if (a->menu()) {
                        if (QAction* f = findAction(a->menu(), id)) return f;
                    } else if (!a->isSeparator() && a->data().toUInt() == id)
                        return a;
                }
                return nullptr;
            };
            for (const QString& c : commands.split(',', Qt::SkipEmptyParts)) {
                uint id = c.trimmed().toUInt(nullptr, 0);
                QAction* action = nullptr;
                for (QAction* top : window.menuBar()->actions())
                    if (top->menu() && (action = findAction(top->menu(), id))) break;
                if (action)
                    action->trigger();
                else
                    g_rmtHost->PostCommand(id); // not in the menu (e.g. accelerators): WM_COMMAND
                QCoreApplication::processEvents();
            }
        });
        int grabMs = qEnvironmentVariableIsSet("RMT_QT_GRAB_MS") ? qEnvironmentVariableIntValue("RMT_QT_GRAB_MS") : 1000;
        QTimer::singleShot(grabMs, &window, [&window, grab] {
            window.grab().save(grab);
            // RMT_QT_MENU_GRAB=prefix: every top level menu as prefix-<title>.png
            // (the screenshots of the manual)
            QString menuGrab = qEnvironmentVariable("RMT_QT_MENU_GRAB");
            if (!menuGrab.isEmpty()) {
                for (QAction* top : window.menuBar()->actions()) {
                    QMenu* menu = top->menu();
                    if (!menu) continue;
                    QString name = top->text().remove('&');
                    menu->popup(QPoint(0, 0)); // shown, so the keys and the greyed items are drawn
                    QCoreApplication::processEvents();
                    menu->grab().save(menuGrab + "-" + name + ".png");
                    menu->close();
                }
            }
            QCoreApplication::exit(0);
        });
    }

    window.Start(SongFromCommandLine(argc, argv));
    if (scaleOverride) window.OverrideScaling(scaleOverride); // this session only
    // a first start: the window of the interface size that the first start chose
    if (!windowRestored && !scaleOverride && g_scaling_percentage > 100) DefaultWindow(window, g_scaling_percentage);

    // RMT_QT_MENU_TEST: trigger every menu action (except exit) and verify
    // all have registered handlers (no "has no handler" debug warning).
    // Use together with QT_QPA_PLATFORM=offscreen and RMT_QT_GRAB=<any>.
    if (qEnvironmentVariableIsSet("RMT_QT_MENU_TEST")) {
        static QStringList noHandlerMsgs;
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
            if (msg.contains("has no handler"))
                noHandlerMsgs << msg;
        });

        // IDs that would close or exit the application - skip in tests
        static const QSet<uint> skipIds = {
            (uint)ID_FILE_EXIT,
            (uint)ID_WANTEXIT,
            (uint)ID_APP_EXIT,
        };

        QTimer::singleShot(300, &window, [&window] {
            // Collect all leaf actions (not separators, not submenu headers)
            std::function<QList<QAction*>(QMenu*)> collectLeafs = [&](QMenu* m) -> QList<QAction*> {
                QList<QAction*> result;
                for (QAction* a : m->actions()) {
                    if (a->isSeparator()) continue;
                    if (a->menu())
                        result += collectLeafs(a->menu());
                    else
                        result << a;
                }
                return result;
            };

            QList<QAction*> actions;
            for (QAction* top : window.menuBar()->actions())
                if (top->menu()) actions += collectLeafs(top->menu());

            int triggered = 0;
            for (QAction* a : actions) {
                uint id = a->data().toUInt();
                if (!id || skipIds.contains(id)) continue;
                a->trigger();
                QCoreApplication::processEvents();
                ++triggered;
            }
            fprintf(stdout, "RMT_QT_MENU_TEST: triggered %d menu actions\n", triggered);

            QTimer::singleShot(100, [] {
                if (noHandlerMsgs.isEmpty()) {
                    fprintf(stdout, "RMT_QT_MENU_TEST: PASS\n");
                    QCoreApplication::exit(0);
                } else {
                    fprintf(stdout, "RMT_QT_MENU_TEST: FAIL - missing handlers:\n");
                    for (const QString& m : noHandlerMsgs)
                        fprintf(stdout, "  %s\n", m.toLocal8Bit().constData());
                    QCoreApplication::exit(1);
                }
            });
        });
    }

    int result = app.exec();
    // File/Exit already did this (CRmtView::OnWantExit); any other way out
    // (test hooks, QCoreApplication::exit) must not leave the song timer
    // ticking into the destruction of g_Song, nor MIDI IN calling it
    g_Song.StopTimer();
    g_Midi.MidiOff();
    return result;
}
