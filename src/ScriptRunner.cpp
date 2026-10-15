#include "PlatformTypes.h"
#include "ScriptRunner.h"

#include "ASMFileExporter.h"
#include "AssemblerTypes.h"
#include "Atari.h"
#include "AtariBinaries.h"
#include "AtariTrackerDriver.h"
#include "Global.h"
#include "GuiHelpers.h"
#include "Keyboard2NoteMapping.h"
#include "RmtMidi.h"
#include "SAPFile.h"
#include "SAPFileExporter.h"
#include "ScriptMessages.h"
#include "Song.h"
#include "SongContainer.h"
#include "SongExport.h"
#include "SongExporter.h"
#include "TrackerDriverVersion.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define dup2   _dup2
#define fileno _fileno
// <windows.h> would clash with the shim of CompatTypes.h: the one function needed, as it declares it
extern "C" __declspec(dllimport) int __stdcall AttachConsole(unsigned long processId);
#define ATTACH_PARENT_PROCESS ((unsigned long)-1)
#else
#include <unistd.h>
#endif

extern CXPokey g_Pokey;
extern CRmtMidi g_Midi;
extern CAtari g_Atari;
extern CAtariTrackerDriver* g_AtariTrackerDriver;
extern TrackerDriverVersion g_defaultTrackerDriverVersion;
extern CString g_lastLoadPath_Songs;

extern WORD g_rmtstripped_adr_module;
extern BOOL g_rmtstripped_sfx;
extern BOOL g_rmtstripped_gvf;
extern BOOL g_rmtstripped_nos;
extern AssemblerFormat g_AsmFormat;
extern CString g_PrefixForAllAsmLabels;
extern CString g_AsmLabelForStartOfSong;
extern BOOL g_AsmWantRelocatableInstruments;
extern BOOL g_AsmWantRelocatableTracks;
extern BOOL g_AsmWantRelocatableSongLines;
extern CString g_AsmInstrumentsLabel;
extern CString g_AsmTracksLabel;
extern CString g_AsmSongLinesLabel;
extern CString g_rmtmsxtext;
extern BOOL g_msxcheck;
extern BOOL g_msx_shuffle;
extern BOOL g_region_auto;
extern int g_msxcol;

#ifdef RMT_QT_GUI
extern std::string RmtActionTable(int& errorCount); // qt/RmtQtFrontend.cpp
#endif

namespace {

// The Export dialog's file-type list, in its order: script format name, extension, io type.
struct TExportFormat {
    const char* name;
    const char* extension;
    SongIOType ioType;
    std::vector<std::string> options;
    // Whether the format is made by playing the song on the emulated Atari, which needs the driver of the selected
    // version: without it the POKEY stream these formats are built from is another driver's, or silence. The others
    // write the song data itself.
    bool rendersSound;
};

const std::vector<TExportFormat>& ExportFormats()
{
    static const std::vector<TExportFormat> formats = {
        { "stripped-rmt", ".rmt", SongIOType::RMTSTRIPPED, { "address", "sfx", "gvf", "nos", "asmformat" }, false },
        { "asm", ".asm", SongIOType::ASM, { "type", "notes", "durations", "prefix" }, false },
        { "sapr", ".sapr", SongIOType::SAPR, { "author", "name", "date", "subsongs" }, true },
        { "lzss", ".lzss", SongIOType::LZSS, {}, true },
        { "sap", ".sap", SongIOType::LZSS_SAP, { "author", "name", "date", "subsongs" }, true },
        { "xex", ".xex", SongIOType::LZSS_XEX, { "text", "rasterbar", "shuffle", "region-auto", "color" }, true },
        { "xex-rmt", ".xex", SongIOType::XEX_RMTPLAYER, { "name", "author", "date" }, false },
        { "rmtplayer-asm", ".asm", SongIOType::ASM_RMTPLAYER, { "startlabel", "relocate", "instruments-label", "tracks-label", "songlines-label", "asmformat", "sfx", "gvf", "nos" }, false },
        { "wav", ".wav", SongIOType::WAV, {}, true },
    };
    return formats;
}

// CRmtAtariBinaries::GetMissingTrackerDriverMessage() on one line, for a CScriptError rather than a box
std::string MissingTrackerDriverText(TrackerDriverVersion version)
{
    CString filePath = CRmtAtariBinaries::GetTrackerDriverFilePath(version);
    return "The player routines of the selected tracker driver were not loaded from '" + std::string((LPCTSTR)filePath) + "', so RITMO would export another driver's sound, or silence.";
}

std::string Lower(std::string s)
{
    for (char& c : s) {
        c = (char)tolower((unsigned char)c);
    }
    return s;
}

std::string Join(const std::vector<std::string>& items, const char* separator)
{
    std::string result;
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) {
            result += separator;
        }
        result += items[i];
    }
    return result;
}

bool EndsWithNoCase(const std::string& s, const std::string& suffix)
{
    if (s.size() < suffix.size()) {
        return false;
    }
    return Lower(s.substr(s.size() - suffix.size())) == Lower(suffix);
}

bool ParseBoolean(const TScriptCommand& c, const std::string& name, const std::string& value)
{
    std::string v = Lower(value);
    if (v == "yes" || v == "true" || v == "on" || v == "1") {
        return true;
    }
    if (v == "no" || v == "false" || v == "off" || v == "0") {
        return false;
    }
    throw CScriptError(c.line, "'" + name + "' must be yes or no, not '" + value + "'.");
}

bool OptionBoolean(const TScriptCommand& c, const std::string& name, BOOL defaultValue)
{
    return c.HasOption(name) ? ParseBoolean(c, name, c.GetOption(name, "")) : defaultValue != 0;
}

int ParseAddress(const TScriptCommand& c, const std::string& name, const std::string& value)
{
    std::string v = value;
    int base = 10;
    if (!v.empty() && v[0] == '$') {
        v = v.substr(1);
        base = 16;
    } else if (v.size() > 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) {
        v = v.substr(2);
        base = 16;
    }
    char* end = nullptr;
    long result = v.empty() ? -1 : strtol(v.c_str(), &end, base);
    if (v.empty() || *end != 0 || result < 0 || result > 0xFFFF) {
        throw CScriptError(c.line, "'" + name + "' must be an address like $4000, not '" + value + "'.");
    }
    return (int)result;
}

int ParseInt(const TScriptCommand& c, const std::string& name, const std::string& value, int min, int max)
{
    char* end = nullptr;
    long result = value.empty() ? min - 1 : strtol(value.c_str(), &end, 10);
    if (value.empty() || *end != 0 || result < min || result > max) {
        throw CScriptError(c.line, "'" + name + "' must be a number from " + std::to_string(min) + " to " + std::to_string(max) + ", not '" + value + "'.");
    }
    return (int)result;
}

// 1-based index of value among choices (case-insensitive).
int ParseChoice(const TScriptCommand& c, const std::string& name, const std::string& value, const std::vector<std::string>& choices)
{
    for (size_t i = 0; i < choices.size(); i++) {
        if (Lower(choices[i]) == Lower(value)) {
            return (int)i + 1;
        }
    }
    throw CScriptError(c.line, "'" + name + "' must be one of " + Join(choices, ", ") + ", not '" + value + "'.");
}

AssemblerFormat ParseAssemblerFormat(const TScriptCommand& c, const std::string& value)
{
    return ParseChoice(c, "asmformat", value, { "xasm", "atasm" }) == 1 ? XASM : ATASM;
}

// An option value as a text with \n (the XEX screen text has up to 5 lines).
std::string UnescapeLines(std::string value)
{
    std::string result;
    for (size_t i = 0; i < value.size(); i++) {
        if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == 'n') {
            result += '\n';
            i++;
        } else {
            result += value[i];
        }
    }
    return result;
}

CString ToCString(const std::string& s)
{
    return CString(s.c_str());
}

CString PathString(const std::filesystem::path& p)
{
    return CString(p.string().c_str());
}

TrackerDriverVersion ParseDriverVersion(const TScriptCommand& c, const std::string& value)
{
    static const std::vector<std::pair<std::string, TrackerDriverVersion>> versions = {
        { "unpatched", TrackerDriverVersion::UNPATCHED },
        { "unpatched-with-tuning", TrackerDriverVersion::UNPATCHED_WITH_TUNING },
        { "patch3", TrackerDriverVersion::PATCH3 },
        { "patch6", TrackerDriverVersion::PATCH6 },
        { "patch8", TrackerDriverVersion::PATCH8 },
        { "patch16", TrackerDriverVersion::PATCH16 },
        { "patch-prince-of-persia", TrackerDriverVersion::PATCH_PRINCE_OF_PERSIA },
    };
    std::string wanted = Lower(value);
    for (char& ch : wanted) {
        if (ch == '_') {
            ch = '-';
        }
    }
    std::vector<std::string> names;
    for (const auto& v : versions) {
        if (v.first == wanted) {
            return v.second;
        }
        names.push_back(v.first);
    }
    throw CScriptError(c.line, "'driver' must be one of " + Join(names, ", ") + ", not '" + value + "'.");
}

void RequireArguments(const TScriptCommand& c, size_t count, const char* usage)
{
    if (c.arguments.size() < count) {
        throw CScriptError(c.line, std::string("Usage: ") + usage);
    }
    if (c.arguments.size() > count) {
        throw CScriptError(c.line, "Unexpected argument '" + c.arguments[count] + "' (a file name with blanks needs quotes). Usage: " + usage);
    }
}

void RequireNoOptions(const TScriptCommand& c)
{
    if (!c.options.empty()) {
        throw CScriptError(c.line, "The command '" + c.name + "' takes no options.");
    }
}

} // namespace

void RedirectScriptOutputToFile(const CString& logPath)
{
    // One file, one offset for both streams (a second freopen would keep its
    // own position and the two would overwrite each other's lines).
    FILE* stream = freopen(logPath, "w", stdout);
    if (stream == nullptr) {
        return;
    }
    setvbuf(stdout, nullptr, _IONBF, 0);
    // stderr follows where it can, although nothing of the script's own goes there any more (see
    // CScriptRunner::Err()): the LZSS exporter prints its statistics to it
    dup2(fileno(stdout), fileno(stderr));
    setvbuf(stderr, nullptr, _IONBF, 0);
}

void AttachScriptConsole(const CString& scriptFilePath)
{
    // RMT_SCRIPT_LOG=<file>: everything into that file, console or not
    const char* logOverride = getenv("RMT_SCRIPT_LOG");
    if (logOverride != nullptr && *logOverride) {
        RedirectScriptOutputToFile(logOverride);
        return;
    }
#ifdef _WIN32
    // A windowed program has no console of its own: the parent's, else <script>.log
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        printf("\n"); // the prompt of the parent console is on the current line
    } else {
        RedirectScriptOutputToFile(scriptFilePath + ".log");
    }
#else
    (void)scriptFilePath;
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif
}

CScriptRunner::CScriptRunner(CSong& song) : m_song(song)
{
}

int CScriptRunner::RunFile(const CString& scriptFilePath)
{
    std::ifstream in(scriptFilePath, std::ios::binary);
    if (!in) {
        printf("The script file '%s' cannot be read.\n", (LPCTSTR)scriptFilePath); // stdout - see Err()
        return EXIT_SCRIPT_INVALID;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::vector<TScriptCommand> commands;
    try {
        commands = CScriptParser::Parse(CScriptParser::SplitLines(buffer.str()));
    } catch (const CScriptError& e) {
        std::filesystem::path p((LPCTSTR)scriptFilePath);
        printf("%s: %s\n", p.filename().string().c_str(), e.GetLocatedMessage().c_str()); // stdout - see Err()
        return EXIT_SCRIPT_INVALID;
    }
    std::filesystem::path folder = std::filesystem::absolute(std::filesystem::path((LPCTSTR)scriptFilePath)).parent_path();
    return Run(commands, folder);
}

int CScriptRunner::RunInteractive(const CString& scriptFilePath, std::string& output)
{
    m_interactive = true;
    m_capture = &output;
    int code;
    std::ifstream in(scriptFilePath, std::ios::binary);
    if (!in) {
        Err("The script file '" + std::string((LPCTSTR)scriptFilePath) + "' cannot be read.");
        code = EXIT_SCRIPT_INVALID;
    } else {
        std::stringstream buffer;
        buffer << in.rdbuf();
        try {
            std::vector<TScriptCommand> commands = CScriptParser::Parse(CScriptParser::SplitLines(buffer.str()));
            std::filesystem::path folder = std::filesystem::absolute(std::filesystem::path((LPCTSTR)scriptFilePath)).parent_path();
            code = Run(commands, folder);
        } catch (const CScriptError& e) {
            Err(e.GetLocatedMessage());
            code = EXIT_SCRIPT_INVALID;
        }
    }
    m_capture = nullptr;
    m_interactive = false;
    return code;
}

void CScriptRunner::Out(const std::string& line)
{
    printf("%s\n", line.c_str());
    if (m_capture != nullptr) {
        *m_capture += line + "\n";
    }
}

void CScriptRunner::Err(const std::string& line)
{
    // stdout, not stderr: on Windows RITMO is a windowed program, which starts with no stderr stream, and when the
    // output goes to <script>.log neither freopen() nor dup2() gives it one - every error line was lost there, and
    // a failed command left nothing but the exit code (ported from RMT 1.36, 19691e2). The output of a script is
    // one stream either way.
    printf("%s\n", line.c_str());
    if (m_capture != nullptr) {
        *m_capture += line + "\n";
    }
}

int CScriptRunner::Run(const std::vector<TScriptCommand>& commands, const std::filesystem::path& baseFolder)
{
    m_baseFolder = baseFolder;
    SetScriptMessageMode(true, m_interactive);
    int code = EXIT_OK;
    for (const TScriptCommand& command : commands) {
        ClearScriptProblems();
        try {
            if (!Execute(command)) {
                break; // quit
            }
        } catch (const CScriptError& e) {
            Err(e.GetLocatedMessage());
            code = EXIT_COMMAND_FAILED;
            break;
        } catch (const std::exception& e) {
            Err("line " + std::to_string(command.line) + ": " + command.name + " failed: " + e.what());
            code = EXIT_COMMAND_FAILED;
            break;
        }
    }
    SetScriptMessageMode(false);
    fflush(stdout);
    fflush(stderr);
    return code;
}

bool CScriptRunner::Execute(const TScriptCommand& command)
{
    if (command.name == "open") {
        Open(command);
    } else if (command.name == "save") {
        Save(command);
    } else if (command.name == "export") {
        Export(command);
    } else if (command.name == "set") {
        Set(command);
    } else if (command.name == "dump") {
        Dump(command);
    } else if (command.name == "midi") {
        Midi(command);
    } else if (command.name == "echo") {
        Out(Join(command.arguments, " "));
    } else if (command.name == "quit") {
        return false;
    } else {
        throw CScriptError(command.line, "Unknown command '" + command.name + "'.");
    }
    return true;
}

void CScriptRunner::SetOutputFolder(const std::filesystem::path& outputFolder)
{
    m_outputOverride = outputFolder;
}

std::filesystem::path CScriptRunner::Resolve(const std::string& path) const
{
    return (m_baseFolder / std::filesystem::path(path)).lexically_normal();
}

std::filesystem::path CScriptRunner::ResolveOutput(const TScriptCommand& command, const std::string& path) const
{
    const std::filesystem::path& base = !m_outputOverride.empty() ? m_outputOverride : !m_outputFolder.empty() ? m_outputFolder
                                                                                                               : m_baseFolder;
    std::filesystem::path file = (base / std::filesystem::path(path)).lexically_normal();
    std::error_code error;
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path(), error);
    }
    if (error) {
        throw CScriptError(command.line, "Cannot create the folder '" + file.parent_path().string() + "': " + error.message());
    }
    return file;
}

void CScriptRunner::CheckOverwrite(const TScriptCommand& command, const std::filesystem::path& file) const
{
    if (!m_overwrite && std::filesystem::exists(file)) {
        throw CScriptError(command.line, "'" + file.string() + "' exists already (use 'set overwrite yes' to replace files).");
    }
}

std::string CScriptRunner::Problems()
{
    std::string problems = GetScriptProblems();
    return problems.empty() ? "" : " " + problems;
}

void CScriptRunner::Open(const TScriptCommand& command)
{
    RequireArguments(command, 1, "open <file>");
    RequireNoOptions(command);
    std::filesystem::path file = Resolve(command.GetArgument(0));
    if (!m_song.FileOpen(PathString(file), FALSE) || !GetScriptProblems().empty()) {
        throw CScriptError(command.line, "Cannot open '" + file.string() + "'." + Problems());
    }
    Out("Opened " + file.string());
}

void CScriptRunner::Save(const TScriptCommand& command)
{
    RequireArguments(command, 1, "save <file>");
    RequireNoOptions(command);
    std::filesystem::path file = ResolveOutput(command, command.GetArgument(0));
    std::string name = file.string();
    SongIOType ioType;
    if (EndsWithNoCase(name, ".rmt")) {
        ioType = SongIOType::RMT;
    } else if (EndsWithNoCase(name, ".txt")) {
        ioType = SongIOType::TXT;
    } else if (EndsWithNoCase(name, ".rmw")) {
        ioType = SongIOType::RMW;
    } else {
        throw CScriptError(command.line, "The file name must end in .rmt, .txt or .rmw.");
    }
    CheckOverwrite(command, file);
    // FileSaveAs() without its dialog: the name and type, then FileSave()
    m_song.SetLoadedFile(PathString(file), ioType);
    g_lastLoadPath_Songs = PathString(file.parent_path());
    m_song.FileSave();
    if (!GetScriptProblems().empty() || !std::filesystem::is_regular_file(file)) {
        throw CScriptError(command.line, "Saving '" + file.string() + "' failed." + Problems());
    }
    Out("Saved " + file.string());
}

void CScriptRunner::Export(const TScriptCommand& command)
{
    RequireArguments(command, 2, "export <format> <file> [name=value ...]");
    std::string formatName = Lower(command.GetArgument(0));
    const TExportFormat* format = nullptr;
    std::vector<std::string> formatNames;
    for (const TExportFormat& f : ExportFormats()) {
        if (formatName == f.name) {
            format = &f;
        }
        formatNames.push_back(f.name);
    }
    if (format == nullptr) {
        throw CScriptError(command.line, "Unknown export format '" + command.GetArgument(0) + "'; one of " + Join(formatNames, ", ") + ".");
    }
    // the export would otherwise run to the end and write the sound of another driver, or a quiet file
    if (format->rendersSound && !g_AtariTrackerDriver->AreRoutinesLoaded()) throw CScriptError(command.line, MissingTrackerDriverText(m_song.GetTrackerDriverVersion()));
    for (const auto& option : command.options) {
        bool known = false;
        for (const std::string& allowed : format->options) {
            known |= allowed == option.first;
        }
        if (!known) {
            std::vector<std::string> sorted = format->options;
            std::sort(sorted.begin(), sorted.end());
            throw CScriptError(command.line, "Unknown option '" + option.first + "' for the format '" + formatName + "'" + (sorted.empty() ? std::string(" (it has none).") : "; one of " + Join(sorted, ", ") + "."));
        }
    }
    std::filesystem::path file = ResolveOutput(command, command.GetArgument(1));
    if (!EndsWithNoCase(file.string(), format->extension)) {
        file += format->extension;
    }
    CheckOverwrite(command, file);
    CExportSection section(m_song); // no timer thread while the exporters drive the tracker driver and the POKEY

    // CSong::FileExportAs() without its dialogs
    m_song.Stop();
    if (!m_song.TestBeforeFileSave()) {
        MessageBox(g_hwnd, "Warning!\nNo data has been saved!", "Warning", MB_ICONEXCLAMATION);
        throw CScriptError(command.line, "Exporting '" + file.string() + "' as " + formatName + " failed." + Problems());
    }
    CString fn = PathString(file);
    g_lastLoadPath_Songs = PathString(file.parent_path());
    std::ofstream out(fn, std::ios::binary);
    if (!out) {
        throw CScriptError(command.line, "Can't create this file: " + file.string());
    }
    m_song.SetLastExportIOType(format->ioType);
    auto started = std::chrono::steady_clock::now();

    // CSong::ExportV2() with the dialogs' answers taken from the options
    TExportDescription exportDesc{};
    exportDesc.targetAddrOfModule = 0x4000;
    int maxAddr = m_song.MakeModule(exportDesc.mem, exportDesc.targetAddrOfModule, format->ioType, exportDesc.instrumentSavedFlags, exportDesc.trackSavedFlags);
    bool exportResult = false;
    if (maxAddr >= 0) {
        exportDesc.firstByteAfterModule = maxAddr;
        CSongContainer songContainer(m_song);
        CSongExporter songExporter;
        CSongExport songExport(songContainer, fn);
        const TScriptCommand& c = command;
        switch (format->ioType) {
            case SongIOType::RMTSTRIPPED: {
                // CRmtExporter::ExportAsStrippedRMT: the dialog's fields, saved for later reuse as the dialog does
                g_rmtstripped_adr_module = (WORD)(c.HasOption("address") ? ParseAddress(c, "address", c.GetOption("address", "")) : g_rmtstripped_adr_module);
                g_rmtstripped_sfx = OptionBoolean(c, "sfx", g_rmtstripped_sfx);
                g_rmtstripped_gvf = OptionBoolean(c, "gvf", g_rmtstripped_gvf);
                g_rmtstripped_nos = OptionBoolean(c, "nos", g_rmtstripped_nos);
                g_AsmFormat = c.HasOption("asmformat") ? ParseAssemblerFormat(c, c.GetOption("asmformat", "")) : g_AsmFormat;
                exportResult = CSong::ExportAsStrippedRMTApply(m_song, out, g_rmtstripped_adr_module, g_rmtstripped_sfx);
                break;
            }
            case SongIOType::ASM: {
                // CASMFileExporter::ExportAsAsm
                int exportType = ParseChoice(c, "type", c.GetOption("type", "tracks"), { "tracks", "song" });
                int notesIndexOrFreq = ParseChoice(c, "notes", c.GetOption("notes", "index"), { "index", "freq" });
                int durationsType = ParseChoice(c, "durations", c.GetOption("durations", "notes"), { "notes", "note-duration", "duration-note" });
                g_PrefixForAllAsmLabels = ToCString(c.GetOption("prefix", (LPCTSTR)g_PrefixForAllAsmLabels));
                exportResult = CASMFileExporter::ExportAsAsmApply(m_song, out, exportType, notesIndexOrFreq, durationsType);
                break;
            }
            case SongIOType::ASM_RMTPLAYER: {
                // CASMFileExporter::ExportAsRelocatableAsmForRmtPlayer
                TExportDescription exportDescWithSFX{};
                exportDescWithSFX.targetAddrOfModule = 0x4000;
                exportDescWithSFX.firstByteAfterModule = m_song.MakeModule(exportDescWithSFX.mem, exportDescWithSFX.targetAddrOfModule, SongIOType::RMT, exportDescWithSFX.instrumentSavedFlags, exportDescWithSFX.trackSavedFlags);
                if (exportDescWithSFX.firstByteAfterModule < 0) {
                    break;
                }
                TRelocatableAsmExportParams params;
                params.strAsmLabelForStartOfSong = ToCString(c.GetOption("startlabel", g_AsmLabelForStartOfSong.IsEmpty() ? "RMT_SONG_DATA" : (LPCTSTR)g_AsmLabelForStartOfSong));
                params.wantRelocatableInstruments = g_AsmWantRelocatableInstruments;
                params.wantRelocatableTracks = g_AsmWantRelocatableTracks;
                params.wantRelocatableSongLines = g_AsmWantRelocatableSongLines;
                if (c.HasOption("relocate")) {
                    params.wantRelocatableInstruments = params.wantRelocatableTracks = params.wantRelocatableSongLines = FALSE;
                    std::stringstream parts(c.GetOption("relocate", ""));
                    std::string part;
                    while (std::getline(parts, part, ',')) {
                        std::string p = Lower(part);
                        p.erase(0, p.find_first_not_of(' '));
                        p.erase(p.find_last_not_of(' ') + 1);
                        if (p == "instruments") {
                            params.wantRelocatableInstruments = TRUE;
                        } else if (p == "tracks") {
                            params.wantRelocatableTracks = TRUE;
                        } else if (p == "songlines") {
                            params.wantRelocatableSongLines = TRUE;
                        } else if (p != "" && p != "none") {
                            throw CScriptError(c.line, "'relocate' lists instruments, tracks and/or songlines, not '" + part + "'.");
                        }
                    }
                }
                params.strAsmInstrumentsLabel = ToCString(c.GetOption("instruments-label", g_AsmInstrumentsLabel.IsEmpty() ? "RMT_INSTRUMENT_DATA" : (LPCTSTR)g_AsmInstrumentsLabel));
                params.strAsmTracksLabel = ToCString(c.GetOption("tracks-label", g_AsmTracksLabel.IsEmpty() ? "RMT_SONG_TRACKS" : (LPCTSTR)g_AsmTracksLabel));
                params.strAsmSongLinesLabel = ToCString(c.GetOption("songlines-label", g_AsmSongLinesLabel.IsEmpty() ? "RMT_SONG_LINES" : (LPCTSTR)g_AsmSongLinesLabel));
                params.assemblerFormat = c.HasOption("asmformat") ? ParseAssemblerFormat(c, c.GetOption("asmformat", "")) : g_AsmFormat;
                params.sfxSupport = OptionBoolean(c, "sfx", g_rmtstripped_sfx);
                params.globalVolumeFade = OptionBoolean(c, "gvf", g_rmtstripped_gvf);
                params.noStartingSongLine = OptionBoolean(c, "nos", g_rmtstripped_nos);
                // Save the dialog settings for future exports (as the dialog wrapper does)
                g_AsmLabelForStartOfSong = params.strAsmLabelForStartOfSong;
                g_AsmWantRelocatableInstruments = params.wantRelocatableInstruments;
                g_AsmWantRelocatableTracks = params.wantRelocatableTracks;
                g_AsmWantRelocatableSongLines = params.wantRelocatableSongLines;
                g_AsmInstrumentsLabel = params.strAsmInstrumentsLabel;
                g_AsmTracksLabel = params.strAsmTracksLabel;
                g_AsmSongLinesLabel = params.strAsmSongLinesLabel;
                g_AsmFormat = params.assemblerFormat;
                g_rmtstripped_sfx = params.sfxSupport;
                g_rmtstripped_gvf = params.globalVolumeFade;
                g_rmtstripped_nos = params.noStartingSongLine;
                exportResult = CASMFileExporter::ExportAsRelocatableAsmForRmtPlayerApply(m_song, out, &exportDesc, &exportDescWithSFX, params);
                break;
            }
            case SongIOType::SAPR:
            case SongIOType::LZSS_SAP: {
                // CSAPFileExportDialog::Show without the dialog
                CSAPFile sapFile;
                sapFile.SetType(format->ioType == SongIOType::SAPR ? "R" : "B");
                sapFile.Init(m_song);
                CString subsongs;
                m_song.GetSubsongParts(subsongs);
                sapFile.SetAuthor(ToCString(c.GetOption("author", (LPCTSTR)sapFile.GetAuthor())));
                sapFile.SetName(ToCString(c.GetOption("name", (LPCTSTR)sapFile.GetName())));
                sapFile.SetDate(ToCString(c.GetOption("date", (LPCTSTR)sapFile.GetDate())));
                std::vector<int> positions;
                sapFile.SetSongs(CSAPFile::ParseSubsongs(ToCString(c.GetOption("subsongs", (LPCTSTR)subsongs)), &positions));
                sapFile.SetSubsongPositions(positions);
                exportResult = format->ioType == SongIOType::SAPR ? CSAPFileExporter::ExportSAP_R(songExport, sapFile, out) : CSAPFileExporter::ExportSAP_B_LZSS(songExport, sapFile, out);
                break;
            }
            case SongIOType::LZSS:
                exportResult = songExporter.ExportLZSS(songExport, out);
                break;
            case SongIOType::LZSS_XEX: {
                // CSongExporter::ShowXEXExportDialog without the dialog
                CXEXFile xexFile;
                xexFile.InitFromSong(m_song);
                CString text = c.HasOption("text") ? ToCString(UnescapeLines(c.GetOption("text", ""))) : CSongExporter::DefaultXexText(xexFile);
                g_rmtmsxtext = text;
                g_rmtmsxtext.Replace("\x0d\x0d", "\x0d"); // 13, 13 => 13
                CSongExporter::SetXexText(xexFile, text);
                g_msxcheck = OptionBoolean(c, "rasterbar", g_msxcheck);
                g_msx_shuffle = OptionBoolean(c, "shuffle", g_msx_shuffle);
                g_region_auto = OptionBoolean(c, "region-auto", g_region_auto);
                g_msxcol = c.HasOption("color") ? ParseInt(c, "color", c.GetOption("color", ""), 0, 255) : g_msxcol;
                xexFile.rasterbarColor = g_msxcol;
                xexFile.displayRasterbar = g_msxcheck != 0;
                xexFile.autoRegion = g_region_auto != 0;
                exportResult = songExporter.ExportXEX_LZSS(songExport, xexFile, out);
                break;
            }
            case SongIOType::XEX_RMTPLAYER: {
                // CSongExporter::ShowRmtXexExportDialog without the dialog: NAME, AUTHOR, DATE (default: from the
                // song name, as the dialog proposes them)
                CRmtXexFile xexFile;
                xexFile.InitFromSong(m_song);
                if (c.HasOption("name")) xexFile.name = ToCString(c.GetOption("name", ""));
                if (c.HasOption("author")) xexFile.author = ToCString(c.GetOption("author", ""));
                if (c.HasOption("date")) xexFile.date = ToCString(c.GetOption("date", ""));
                exportResult = songExporter.ExportXEX_RMTPlayer(songExport, xexFile, out);
                break;
            }
            case SongIOType::WAV:
                exportResult = songExporter.ExportWAV(songExport, out, g_Pokey, g_Atari.GetMemoryAt(0));
                break;
            default:
                break;
        }
    }
    if (out.is_open()) {
        out.close();
    }
    if (!exportResult) {
        DeleteFile(fn);
        CString message;
        message.Format("Incomplete export file '%s' was deleted.", (LPCTSTR)fn);
        MessageBox(g_hwnd, message, "Export aborted", MB_ICONEXCLAMATION);
    }
    if (!exportResult || !GetScriptProblems().empty() || !std::filesystem::is_regular_file(file)) {
        throw CScriptError(command.line, "Exporting '" + file.string() + "' as " + formatName + " failed." + Problems());
    }
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    Out("Exported " + file.string() + " (" + std::to_string(ms) + " ms)");
}

void CScriptRunner::Set(const TScriptCommand& command)
{
    RequireArguments(command, 2, "set <name> <value>");
    RequireNoOptions(command);
    std::string name = Lower(command.GetArgument(0));
    std::string value = command.GetArgument(1);
    if (name == "overwrite") {
        m_overwrite = ParseBoolean(command, "overwrite", value);
    } else if (name == "output") {
        m_outputFolder = Resolve(value);
    } else if (name == "ntsc") {
        // CRmtView::SetNTSC (the Options dialog's NTSC box)
        bool ntsc = ParseBoolean(command, "ntsc", value);
        if ((m_song.IsNTSC() != 0) != ntsc) {
            m_song.GetTuning().basetuning = (ntsc) ? (m_song.GetTuning().basetuning * CAtari::FREQ_17_NTSC) / CAtari::FREQ_17_PAL : (m_song.GetTuning().basetuning * CAtari::FREQ_17_PAL) / CAtari::FREQ_17_NTSC;
            m_song.SetNTSC(ntsc);
        }
    } else if (name == "driver") {
        // the Module Properties' driver: the default and the song's
        TrackerDriverVersion version = ParseDriverVersion(command, value);
        g_defaultTrackerDriverVersion = version;
        if (m_song.GetTrackerDriverVersion() != version) {
            m_song.SetTrackerDriverVersion(version);
            g_Atari.Init(m_song.IsNTSC());
            g_AtariTrackerDriver->LoadRMTRoutines(version);
            if (!g_AtariTrackerDriver->AreRoutinesLoaded()) throw CScriptError(command.line, MissingTrackerDriverText(version));
        }
    } else if (name == "midi-touch-response") {
        // the Options dialog's MIDI settings (for the midi command: the same recording rules as a real device)
        g_Midi.m_TouchResponse = ParseBoolean(command, "midi-touch-response", value);
    } else if (name == "midi-volume-offset") {
        g_Midi.m_VolumeOffset = ParseInt(command, "midi-volume-offset", value, 0, 15);
    } else if (name == "midi-note-off") {
        g_Midi.m_NoteOff = ParseBoolean(command, "midi-note-off", value);
    } else {
        throw CScriptError(command.line, "Unknown setting '" + command.GetArgument(0) + "'; one of overwrite, output, ntsc, driver, midi-touch-response, midi-volume-offset, midi-note-off.");
    }
}

// A MIDI byte: two hex digits (optionally with $ or 0x).
static int ParseMidiByte(const TScriptCommand& c, const std::string& value)
{
    std::string v = value;
    if (!v.empty() && v[0] == '$') {
        v = v.substr(1);
    } else if (v.size() > 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) {
        v = v.substr(2);
    }
    char* end = nullptr;
    long result = v.empty() ? -1 : strtol(v.c_str(), &end, 16);
    if (v.empty() || *end != 0 || result < 0 || result > 0xFF) {
        throw CScriptError(c.line, "'" + value + "' is not a MIDI byte (two hex digits, 00 to FF).");
    }
    return (int)result;
}

// midi <status> <data1> [<data2>]: a MIDI message as hex bytes (e.g. "midi 90 3C 64", note on C-3 velocity 100 on
// channel 1), handled exactly as one from the MIDI IN device (CSong::MidiEvent), with the window counted as focused.
void CScriptRunner::Midi(const TScriptCommand& command)
{
    if (command.arguments.size() < 2 || command.arguments.size() > 3) {
        throw CScriptError(command.line, "Usage: midi <status> <data1> [<data2>] (hex bytes).");
    }
    RequireNoOptions(command);
    DWORD status = ParseMidiByte(command, command.GetArgument(0));
    DWORD data1 = ParseMidiByte(command, command.GetArgument(1));
    DWORD data2 = command.arguments.size() > 2 ? ParseMidiByte(command, command.GetArgument(2)) : 0;
    m_song.MidiEvent(status | (data1 << 8) | (data2 << 16), true);
}

void CScriptRunner::Dump(const TScriptCommand& command)
{
    RequireArguments(command, 2, "dump actions|notekeys <file>");
    RequireNoOptions(command);
    std::string what = Lower(command.GetArgument(0));
    if (what != "actions" && what != "notekeys") {
        throw CScriptError(command.line, "Unknown dump '" + command.GetArgument(0) + "'; one of actions, notekeys.");
    }
    std::filesystem::path file = ResolveOutput(command, command.GetArgument(1));
    CheckOverwrite(command, file);
    if (what == "notekeys") {
        // The note keys ("tonekeys") of both keyboard layouts (doc/rmt_note_keys.md)
        std::string text = NoteKeysTable();
        std::ofstream out(file, std::ios::binary);
        out.write(text.data(), text.size());
        out.close();
        Out("Dumped the note keys to " + file.string());
        return;
    }
    // The command table of the program: every menu item and toolbar button with its key and prompt. The Qt
    // frontend collects it from its menus; an ERROR marker (a label that shows another key than the item has) fails the command.
#ifdef RMT_QT_GUI
    int errors = 0;
    std::string text = RmtActionTable(errors);
    std::ofstream out(file, std::ios::binary);
    out.write(text.data(), text.size());
    out.close();
    Out("Dumped the actions to " + file.string());
    if (errors > 0) {
        throw CScriptError(command.line, "The action table has " + std::to_string(errors) + " ERROR marker(s), see " + file.string() + ".");
    }
#else
    throw CScriptError(command.line, "'dump actions' is available in the Qt frontend only (use 'dump notekeys').");
#endif
}
