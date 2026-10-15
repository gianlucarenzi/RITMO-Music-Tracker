#include "PlatformTypes.h"
#include "ScriptMessages.h"

#include <cstdio>

namespace {
bool g_scriptMessageMode = false;
bool g_scriptInteractive = false; // script mode with the window: boxes shown, problems still collected
std::string g_scriptProblems;

// One line: "title: message" with the message's line ends flattened.
std::string OneLine(const char* title, const char* message)
{
    std::string text = message ? message : "";
    for (char& c : text) {
        if (c == '\r' || c == '\n') {
            c = ' ';
        }
    }
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return (title && *title ? std::string(title) + ": " : std::string()) + text;
}
} // namespace

void SetScriptMessageMode(bool enabled, bool interactive)
{
    g_scriptMessageMode = enabled;
    g_scriptInteractive = enabled && interactive;
    g_scriptProblems.clear();
}

void ClearScriptProblems()
{
    g_scriptProblems.clear();
}

std::string GetScriptProblems()
{
    std::string result = g_scriptProblems;
    while (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

bool ScriptMessageBox(const char* text, const char* caption, unsigned type, int& answer)
{
    if (!g_scriptMessageMode) {
        return false;
    }
    const unsigned buttons = type & 0x0F;
    const unsigned icon = type & 0xF0;
    const bool question = buttons == MB_YESNO || buttons == MB_YESNOCANCEL || buttons == MB_OKCANCEL;
    std::string line = OneLine(caption, text);
    if (!question && icon != MB_ICONINFORMATION) {
        g_scriptProblems += line + "\n"; // fails the command, in both script modes
    }
    if (g_scriptInteractive) {
        return false; // the box is shown as always
    }
    if (question) {
        printf("%s (a script answers No)\n", line.c_str()); // stdout - see CScriptRunner::Err()
        answer = buttons == MB_OKCANCEL ? IDCANCEL : IDNO;
    } else {
        printf("%s\n", line.c_str()); // the problems too: stdout - see CScriptRunner::Err()
        answer = IDOK;
    }
    return true;
}
