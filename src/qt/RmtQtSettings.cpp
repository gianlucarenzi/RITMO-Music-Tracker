// RmtQtSettings.cpp - the configuration of the Qt frontend in QSettings
//
// CRmtView (RmtView.cpp) reads and writes the configuration and the tuning
// as the "NAME = value" lines of ritmo.ini / tuning.ini; here each line is a
// key of QSettings, in the group named after the file ("ritmo", "tuning"). The
// settings belong to the user on the host system, not to the program folder,
// so an update or a new installation of RITMO keeps them:
//   Linux    ~/.config/ritmo-atari.org/ritmo.conf ($XDG_CONFIG_HOME)
//   Windows  registry, HKEY_CURRENT_USER\Software\ritmo-atari.org\ritmo
//   macOS    ~/Library/Preferences/org.ritmo-atari.ritmo.plist
// With nothing saved yet, the settings of RMT (organization raster-atari.org, "rmt" in
// place of "ritmo", group "rmt" for the configuration) and then a ritmo.ini /
// rmt.ini / tuning.ini next to the program (the earlier versions kept them
// there) are taken over once.

#include "StdAfx.h"
#include "Global.h"
#include "RmtQtScreen.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QInputMethod>
#include <QByteArray>
#include <QLocale>
#include <QScreen>
#include <QSettings>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

static QSettings& Settings()
{
    static QSettings settings(QSettings::NativeFormat, QSettings::UserScope, "ritmo-atari.org", "ritmo");
    return settings;
}

// The settings of RMT, before the name RITMO
static QSettings& SettingsRmt()
{
    static QSettings settings(QSettings::NativeFormat, QSettings::UserScope, "raster-atari.org", "rmt");
    return settings;
}

static QString Group(const char* fileName)
{
    return QFileInfo(QString::fromLocal8Bit(fileName)).completeBaseName();
}

// The keys of a group as "NAME = value" lines; false if the group is empty
static bool ReadGroup(QSettings& settings, const QString& group, std::string& text)
{
    settings.beginGroup(group);
    const QStringList keys = settings.childKeys();
    std::ostringstream lines;
    for (const QString& key : keys)
        lines << key.toLocal8Bit().constData() << " = " << settings.value(key).toString().toLocal8Bit().constData() << "\n";
    settings.endGroup();
    if (keys.isEmpty()) return false;
    text = lines.str();
    return true;
}

static bool ReadFile(const char* fileName, std::string& text)
{
    std::ifstream in(GetResourceFilePath(std::filesystem::path(""), fileName));
    if (!in) return false;
    std::ostringstream all;
    all << in.rdbuf();
    text = all.str();
    return true;
}

// RMT_CONFIG_DIR=<folder>: the configuration is the files ritmo.ini and tuning.ini of that folder, as text, instead of
// the settings of the user (QSettings), with nothing taken over from RMT or an earlier version: a run with its own
// settings, isolated from the user's on every system (as in RMT 1.36, plan 47). The window geometry stays where it is.
static std::filesystem::path ConfigDirFile(const char* fileName)
{
    const QString folder = qEnvironmentVariable("RMT_CONFIG_DIR");
    if (folder.isEmpty()) return {};
    return std::filesystem::path(folder.toLocal8Bit().constData()) / fileName;
}

bool RmtLoadConfigText(const char* fileName, std::string& text)
{
    const std::filesystem::path override = ConfigDirFile(fileName);
    if (!override.empty()) {
        std::ifstream in(override, std::ios::binary);
        if (!in) return false; // the defaults (and, outside a script run, the file written with them)
        std::ostringstream all;
        all << in.rdbuf();
        text = all.str();
        return true;
    }

    if (ReadGroup(Settings(), Group(fileName), text)) return true;

    // First start with RITMO: the settings of RMT, then the file of an earlier version
    const bool isConfig = strcmp(fileName, CONFIG_FILENAME) == 0;
    const char* oldName = isConfig ? CONFIG_FILENAME_RMT : fileName;
    if (ReadGroup(SettingsRmt(), Group(oldName), text)) return true;
    if (ReadFile(fileName, text)) return true;
    return isConfig && ReadFile(CONFIG_FILENAME_RMT, text);
}

bool RmtSaveConfigText(const char* fileName, const std::string& text)
{
    const std::filesystem::path override = ConfigDirFile(fileName);
    if (!override.empty()) {
        std::ofstream out(override, std::ios::binary);
        out << text;
        return (bool)out;
    }

    QSettings& settings = Settings();
    settings.remove(Group(fileName));
    settings.beginGroup(Group(fileName));
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        // "NAME = value", as CRmtView reads it: the value starts after "= "
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string name = line.substr(0, eq - 1);
        std::string value = eq + 2 <= line.size() ? line.substr(eq + 2) : "";
        settings.setValue(QString::fromLocal8Bit(name.c_str()), QString::fromLocal8Bit(value.c_str()));
    }
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

CString RmtConfigTextLocation(const char* fileName)
{
    const std::filesystem::path override = ConfigDirFile(fileName);
    if (!override.empty()) return CString(override.string().c_str());
    return CString((Settings().fileName() + " [" + Group(fileName) + "]").toLocal8Bit().constData());
}

// The layout RITMO starts with when nothing is saved yet: the language of the
// keyboard (Qt's input method), German QWERTZ, French AZERTY, else QWERTY.
KeyboardLayout RmtDefaultKeyboardLayout()
{
    QLocale locale = QLocale::system();
    if (QGuiApplication::inputMethod()) {
        locale = QGuiApplication::inputMethod()->locale();
    }
    switch (locale.language()) {
    case QLocale::German:
        return KeyboardLayout::QWERTZ;
    case QLocale::French:
        return KeyboardLayout::AZERTY;
    default:
        return KeyboardLayout::QWERTY;
    }
}

// The interface size of a first start, from the size of the screen (RmtQtScreen.h). The platform
// without a screen of its own (offscreen: the tests, the scripts, the screenshots) keeps 100 %.
int RmtDefaultScalingPercentage()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen || QGuiApplication::platformName() == QLatin1String("offscreen")) return 100;
    const QRect area = screen->availableGeometry();
    return RmtScalingForScreen(area.width(), area.height());
}

// The size and the position of the window, kept for the next start (the group "window" is not the one
// of the configuration, which is written again from the text of CRmtView)
QByteArray RmtLoadWindowGeometry()
{
    QSettings& settings = Settings();
    settings.beginGroup("window");
    const QByteArray geometry = settings.value("geometry").toByteArray();
    settings.endGroup();
    return geometry;
}

void RmtSaveWindowGeometry(const QByteArray& geometry)
{
    QSettings& settings = Settings();
    settings.beginGroup("window");
    settings.setValue("geometry", geometry);
    settings.endGroup();
    settings.sync();
}
