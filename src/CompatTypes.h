// CompatTypes.h
//
// The Windows-style types and free functions (CString, BOOL/WORD/DWORD,
// HWND, COLORREF/RGB, CDC and friends, the message-map macros, the standard
// command IDs) that the RMT engine, the tracker view (RmtView.cpp) and the
// "GUI-shared" sources (GUI_Song.cpp, GUI_Instruments.cpp, GuiHelpers.cpp,
// TracksControl.cpp, ChannelControl.cpp, Global.cpp/h, ...) were written
// with. It is the compatibility layer of RITMO: the program is Qt only, the
// view draws on CompatDC objects that the Qt frontend (qt/) turns into
// pixels, and the window system is reached through IRmtHost.
//
// Where the original code takes a Windows-only branch (loading a DLL,
// creating a DirectSound buffer, opening a real dialog) the code below
// reports "not available" / "cancelled" and the error handling of the engine
// takes care of the rest.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <type_traits>
#include <atomic>
#include <algorithm>
#include <ctime>
#include <system_error>
#include <strings.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Basic Windows SDK typedefs
// ---------------------------------------------------------------------------

// clang-format off
typedef int                BOOL;
typedef unsigned char       BYTE;
typedef unsigned char       byte;
typedef int                 boolean;
typedef unsigned short      WORD;
typedef unsigned long       DWORD;
typedef unsigned int        UINT;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef uintptr_t           DWORD_PTR;
typedef uintptr_t           UINT_PTR;
typedef intptr_t            LONG_PTR;
typedef intptr_t            INT_PTR;
typedef void*               HANDLE;
// Window, instance, cursor and icon handles are pointers to opaque structs,
// as in <windows.h>: the Qt headers of Windows (qwindowdefs_win.h) declare
// them the same way, and a typedef may be repeated only if identical
struct HWND__;      typedef struct HWND__* HWND;
struct HINSTANCE__; typedef struct HINSTANCE__* HINSTANCE;
struct HICON__;     typedef struct HICON__* HICON;
typedef HICON               HCURSOR;
typedef void*               HKEY;
typedef void*               HMENU;
typedef void*               HMIDIIN;
typedef void*               HMIDIOUT;
typedef void*               HGDIOBJ;
typedef void*               HRSRC;
typedef long                HRESULT;
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;
typedef const char*         LPCTSTR;
typedef char*               LPTSTR;
typedef const char*         LPCSTR;
typedef char*               LPTSTR;
typedef char                TCHAR;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
// clang-format on

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL nullptr
#endif

#define CALLBACK
#define WINAPI
#define AFX_MSG
#define afx_msg
#define DECLARE_DYNCREATE(x)
#define IMPLEMENT_DYNCREATE(x, base)
#define DECLARE_DYNAMIC(x)
#define IMPLEMENT_DYNAMIC(x, base)
// DECLARE_MESSAGE_MAP / BEGIN_MESSAGE_MAP ...: see CCmdTarget below
#define TEXT(x) x
#define _T(x)   x

// Standard command IDs of the Windows-style classes (afxres.h), used by the menus and message maps
#define ID_FILE_NEW           0xE100
#define ID_FILE_OPEN          0xE101
#define ID_FILE_CLOSE         0xE102
#define ID_FILE_SAVE          0xE103
#define ID_FILE_SAVE_AS       0xE104
#define ID_FILE_PAGE_SETUP    0xE105
#define ID_FILE_PRINT_SETUP   0xE106
#define ID_FILE_PRINT         0xE107
#define ID_FILE_PRINT_DIRECT  0xE108
#define ID_FILE_PRINT_PREVIEW 0xE109
#define ID_FILE_MRU_FILE1     0xE110
#define ID_EDIT_CLEAR         0xE120
#define ID_EDIT_COPY          0xE122
#define ID_EDIT_CUT           0xE123
#define ID_EDIT_PASTE         0xE125
#define ID_EDIT_SELECT_ALL    0xE12A
#define ID_EDIT_UNDO          0xE12B
#define ID_EDIT_REDO          0xE12C
#define ID_APP_ABOUT          0xE140
#define ID_APP_EXIT           0xE141
#define ID_VIEW_TOOLBAR       0xE800
#define ID_VIEW_STATUS_BAR    0xE801
#define ID_VIEW_REBAR         0xE804
#define AFX_IDW_TOOLBAR       0xE800
#define AFX_IDW_STATUS_BAR    0xE801

#define TRACE(...) ((void)0)

// Dialog / message-box results (subset actually used)
#define IDOK     1
#define IDCANCEL 2
#define IDYES    6
#define IDNO     7

// ---------------------------------------------------------------------------
// Virtual-key codes (subset actually referenced by GUI_Song.cpp/GUI_Instruments.cpp/
// GuiHelpers.cpp/TracksControl.cpp/ChannelControl.cpp), matching the real
// Win32 VK_* numeric values so the mapping stays meaningful once a real
// Qt/X11 keyboard backend is wired up in a later migration phase.
// (VK_BACKSPACE/VK_ENTER/VK_PAGE_UP/VK_PAGE_DOWN are already defined in
// Global.h with the same real values.)
// ---------------------------------------------------------------------------
#define VK_BACK             0x08
#define VK_TAB              0x09
#define VK_ESCAPE           0x1B
#define VK_SPACE            0x20
#define VK_PRIOR            0x21
#define VK_NEXT             0x22
#define VK_END              0x23
#define VK_HOME             0x24
#define VK_LEFT             0x25
#define VK_UP               0x26
#define VK_RIGHT            0x27
#define VK_DOWN             0x28
#define VK_INSERT           0x2D
#define VK_DELETE           0x2E
#define VK_0                0x30
#define VK_1                0x31
#define VK_2                0x32
#define VK_3                0x33
#define VK_4                0x34
#define VK_5                0x35
#define VK_6                0x36
#define VK_7                0x37
#define VK_8                0x38
#define VK_9                0x39
#define VK_A                0x41
#define VK_B                0x42
#define VK_C                0x43
#define VK_D                0x44
#define VK_E                0x45
#define VK_F                0x46
#define VK_G                0x47
#define VK_H                0x48
#define VK_I                0x49
#define VK_J                0x4A
#define VK_K                0x4B
#define VK_L                0x4C
#define VK_M                0x4D
#define VK_N                0x4E
#define VK_O                0x4F
#define VK_P                0x50
#define VK_Q                0x51
#define VK_R                0x52
#define VK_S                0x53
#define VK_T                0x54
#define VK_U                0x55
#define VK_V                0x56
#define VK_W                0x57
#define VK_X                0x58
#define VK_Y                0x59
#define VK_Z                0x5A
#define VK_MULTIPLY         0x6A
#define VK_ADD              0x6B
#define VK_SUBTRACT         0x6D
#define VK_DIVIDE           0x6F
#define VK_F1               0x70
#define VK_F2               0x71
#define VK_F3               0x72
#define VK_F4               0x73
#define VK_CAPITAL          0x14
#define VK_OEM_PLUS         0xBB
#define VK_OEM_MINUS        0xBD
#define VK_RETURN           0x0D
#define VK_SHIFT            0x10
#define VK_CONTROL          0x11
#define VK_MENU             0x12
#define VK_PAUSE            0x13
#define VK_LSHIFT           0xA0
#define VK_RSHIFT           0xA1
#define VK_LCONTROL         0xA2
#define VK_RCONTROL         0xA3
#define VK_LMENU            0xA4
#define VK_RMENU            0xA5
#define VK_NUMPAD0          0x60
#define VK_DECIMAL          0x6E
#define VK_F5               0x74
#define VK_F6               0x75
#define VK_F7               0x76
#define VK_F8               0x77
#define VK_F9               0x78
#define VK_F10              0x79
#define VK_F11              0x7A
#define VK_F12              0x7B
#define VK_NUMLOCK          0x90
#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define VK_MEDIA_STOP       0xB2
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_OEM_1            0xBA
#define VK_OEM_COMMA        0xBC
#define VK_OEM_PERIOD       0xBE
#define VK_OEM_2            0xBF
#define VK_OEM_3            0xC0
#define VK_OEM_4            0xDB
#define VK_OEM_5            0xDC
#define VK_OEM_6            0xDD
#define VK_OEM_7            0xDE
#define VK_OEM_102          0xE2

// mouse key state flags (WM_*BUTTON* wParam)
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT   0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010

// window messages actually posted / sent by the GUI code
#define WM_CLOSE   0x0010
#define WM_COMMAND 0x0111
#define WM_TIMER   0x0113

#define MB_OK              0x00000000L
#define MB_OKCANCEL        0x00000001L
#define MB_YESNO           0x00000004L
#define MB_YESNOCANCEL     0x00000003L
#define MB_ICONERROR       0x00000010L
#define MB_ICONSTOP        0x00000010L
#define MB_ICONWARNING     0x00000030L
#define MB_ICONEXCLAMATION 0x00000030L
#define MB_ICONQUESTION    0x00000020L
#define MB_ICONINFORMATION 0x00000040L

// ---------------------------------------------------------------------------
// COLORREF / RGB
// ---------------------------------------------------------------------------

typedef uint32_t COLORREF;
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))

// ---------------------------------------------------------------------------
// CString - minimal std::string-backed replacement.
//
// Implements exactly the ~20 methods/operators verified (by grep) to be in
// use across the engine + GUI-shared source files. Format() is implemented
// as a variadic *template* (not real C varargs) precisely so that passing
// a CString/std::string argument for a "%s" placeholder works correctly
// (it gets converted to a plain const char* first) - real MFC gets away
// with passing CString by value into "..." only because its internal
// representation is a single pointer; we don't rely on that trick.
// ---------------------------------------------------------------------------

class CString {
public:
    CString() = default;
    CString(const CString&) = default;
    CString(CString&&) = default;
    CString& operator=(const CString&) = default;
    CString& operator=(CString&&) = default;

    CString(const char* s) : m_data(s ? s : "") {}
    CString(char c) : m_data(1, c) {}
    CString(const std::string& s) : m_data(s) {}

    CString& operator=(const char* s)
    {
        m_data = (s ? s : "");
        return *this;
    }

    // --- printf-style formatting -------------------------------------------------
    template <typename... Args>
    void Format(const char* fmt, Args... args)
    {
        m_data = FormatToString(fmt, ConvertArg(args)...);
    }
    template <typename... Args>
    void AppendFormat(const char* fmt, Args... args)
    {
        m_data += FormatToString(fmt, ConvertArg(args)...);
    }

    // --- queries -------------------------------------------------------------
    bool IsEmpty() const { return m_data.empty(); }
    int GetLength() const { return (int)m_data.length(); }
    const char* GetString() const { return m_data.c_str(); }
    char GetAt(int idx) const { return m_data.at((size_t)idx); }
    void SetAt(int idx, char ch) { m_data.at((size_t)idx) = ch; }
    void Empty() { m_data.clear(); }

    CString Left(int count) const
    {
        if (count < 0) count = 0;
        return CString(m_data.substr(0, (size_t)count));
    }
    CString Right(int count) const
    {
        if (count < 0) count = 0;
        size_t len = m_data.length();
        size_t n = (size_t)count > len ? len : (size_t)count;
        return CString(m_data.substr(len - n, n));
    }
    CString Mid(int first) const
    {
        if (first < 0) first = 0;
        if ((size_t)first >= m_data.length()) return CString();
        return CString(m_data.substr((size_t)first));
    }
    CString Mid(int first, int count) const
    {
        if (first < 0) first = 0;
        if (count < 0) count = 0;
        if ((size_t)first >= m_data.length()) return CString();
        return CString(m_data.substr((size_t)first, (size_t)count));
    }

    int Find(char ch, int start = 0) const
    {
        auto pos = m_data.find(ch, (size_t)start);
        return pos == std::string::npos ? -1 : (int)pos;
    }
    int Find(const char* sub, int start = 0) const
    {
        auto pos = m_data.find(sub, (size_t)start);
        return pos == std::string::npos ? -1 : (int)pos;
    }
    int ReverseFind(char ch) const
    {
        auto pos = m_data.rfind(ch);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    int Replace(const char* oldStr, const char* newStr)
    {
        if (!oldStr || !*oldStr) return 0;
        int count = 0;
        size_t pos = 0;
        std::string oldS(oldStr), newS(newStr ? newStr : "");
        while ((pos = m_data.find(oldS, pos)) != std::string::npos) {
            m_data.replace(pos, oldS.length(), newS);
            pos += newS.length();
            count++;
        }
        return count;
    }
    int Replace(char oldCh, char newCh)
    {
        int count = 0;
        for (auto& c : m_data) {
            if (c == oldCh) {
                c = newCh;
                count++;
            }
        }
        return count;
    }

    CString& MakeUpper()
    {
        for (auto& c : m_data) c = (char)toupper((unsigned char)c);
        return *this;
    }
    CString& MakeLower()
    {
        for (auto& c : m_data) c = (char)tolower((unsigned char)c);
        return *this;
    }
    CString& Trim()
    {
        TrimLeftInPlace();
        TrimRightInPlace();
        return *this;
    }
    CString& TrimRight()
    {
        TrimRightInPlace();
        return *this;
    }

    char* GetBuffer(int minLength = 0)
    {
        if (minLength > 0 && (size_t)minLength > m_data.size()) {
            m_data.resize((size_t)minLength);
        }
        return m_data.data();
    }
    void ReleaseBuffer(int newLength = -1)
    {
        if (newLength < 0) {
            m_data.resize(std::strlen(m_data.c_str()));
        } else {
            m_data.resize((size_t)newLength);
        }
    }

    int Compare(const char* other) const { return m_data.compare(other ? other : ""); }
    int CompareNoCase(const char* other) const
    {
        std::string a = m_data, b(other ? other : "");
        for (auto& c : a) c = (char)tolower((unsigned char)c);
        for (auto& c : b) c = (char)tolower((unsigned char)c);
        return a.compare(b);
    }

    // Loads a fixed, small resource-string table (see CompatTypes.cpp) instead
    // of a real Windows .rc string table.
    BOOL LoadString(UINT id);

    // --- conversions / operators ----------------------------------------
    operator const char*() const { return m_data.c_str(); }

    CString& operator+=(const CString& other)
    {
        m_data += other.m_data;
        return *this;
    }
    CString& operator+=(const char* s)
    {
        if (s) m_data += s;
        return *this;
    }
    CString& operator+=(char c)
    {
        m_data += c;
        return *this;
    }

    const std::string& str() const { return m_data; }

private:
    void TrimLeftInPlace()
    {
        size_t i = 0;
        while (i < m_data.size() && std::isspace((unsigned char)m_data[i])) i++;
        m_data.erase(0, i);
    }
    void TrimRightInPlace()
    {
        size_t n = m_data.size();
        while (n > 0 && std::isspace((unsigned char)m_data[n - 1])) n--;
        m_data.erase(n);
    }

    // Helpers for the Format() variadic template: convert CString/std::string
    // arguments to a plain const char* before handing them to vsnprintf-style
    // formatting; every other argument type is passed through unchanged.
    static const char* ConvertArg(const CString& s) { return s.GetString(); }
    static const char* ConvertArg(const std::string& s) { return s.c_str(); }
    template <typename T>
    static T ConvertArg(T v) { return v; }

    template <typename... Args>
    static std::string FormatToString(const char* fmt, Args... args)
    {
        int size = std::snprintf(nullptr, 0, fmt, args...);
        if (size < 0) return std::string();
        std::vector<char> buf((size_t)size + 1);
        std::snprintf(buf.data(), buf.size(), fmt, args...);
        return std::string(buf.data(), (size_t)size);
    }

    std::string m_data;
};

inline CString operator+(const CString& a, const CString& b) { return CString(a.str() + b.str()); }
inline CString operator+(const CString& a, const char* b) { return CString(a.str() + (b ? b : "")); }
inline CString operator+(const char* a, const CString& b) { return CString((a ? a : "") + b.str()); }
inline CString operator+(const CString& a, char b) { return CString(a.str() + b); }

inline bool operator==(const CString& a, const CString& b) { return a.str() == b.str(); }
inline bool operator==(const CString& a, const char* b) { return a.str() == (b ? b : ""); }
inline bool operator==(const char* a, const CString& b) { return b.str() == (a ? a : ""); }
inline bool operator!=(const CString& a, const CString& b) { return !(a == b); }
inline bool operator!=(const CString& a, const char* b) { return !(a == b); }
inline bool operator!=(const char* a, const CString& b) { return !(a == b); }
inline bool operator<(const CString& a, const CString& b) { return a.str() < b.str(); }

// ---------------------------------------------------------------------------
// GDI-ish minimal shims: CPoint / CRect / CPen / CBitmap / CBrush
// ---------------------------------------------------------------------------

struct POINT {
    int x, y;
};
struct RECT {
    int left, top, right, bottom;
};
typedef RECT* LPRECT;
typedef const RECT* LPCRECT;

class CPoint {
public:
    int x = 0, y = 0;
    CPoint() = default;
    CPoint(int x_, int y_) : x(x_), y(y_) {}
    CPoint(const POINT& p) : x(p.x), y(p.y) {}
    CPoint operator+(const CPoint& o) const { return CPoint(x + o.x, y + o.y); }
    CPoint operator-(const CPoint& o) const { return CPoint(x - o.x, y - o.y); }
};

class CRect : public RECT {
public:
    CRect() : RECT{ 0, 0, 0, 0 } {}
    CRect(int l, int t, int r, int b) : RECT{ l, t, r, b } {}
    CRect(const RECT& r) : RECT(r) {}
    CRect(CPoint tl, CPoint br) : RECT{ tl.x, tl.y, br.x, br.y } {}
    BOOL PtInRect(CPoint p) const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
    BOOL IsRectEmpty() const { return right <= left || bottom <= top; }
    void OffsetRect(int dx, int dy)
    {
        left += dx;
        right += dx;
        top += dy;
        bottom += dy;
    }
    void InflateRect(int dx, int dy)
    {
        left -= dx;
        right += dx;
        top -= dy;
        bottom += dy;
    }
    operator LPRECT() { return this; }
    operator LPCRECT() const { return this; }
    void SetRect(int l, int t, int r, int b)
    {
        left = l;
        top = t;
        right = r;
        bottom = b;
    }
    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
    CPoint TopLeft() const { return CPoint(left, top); }
    CPoint BottomRight() const { return CPoint(right, bottom); }
};

#define PS_SOLID     0
#define SRCCOPY      0x00CC0020L
#define HALFTONE     4
#define COLORONCOLOR 3

#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(i) ((LPCTSTR)(uintptr_t)(WORD)(i))
#endif

// Resources compiled into the program (bitmaps of src/res, generated by
// src/CMakeLists.txt from Rmt.rc: see RmtEmbeddedResources.cpp).
// Returns false if the id is unknown.
bool RmtFindResource(UINT id, const unsigned char** data, size_t* size);

// Pixel format of CBitmap / CDC: 0xFFRRGGBB (QImage::Format_RGB32 compatible)
inline uint32_t RmtPixel(COLORREF c)
{
    return 0xFF000000u | ((c & 0xFFu) << 16) | (c & 0xFF00u) | ((c >> 16) & 0xFFu);
}

class CPen {
public:
    CPen() = default;
    CPen(int style, int width, COLORREF color) : m_style(style), m_width(width), m_color(color) {}
    BOOL CreatePen(int style, int width, COLORREF color)
    {
        m_style = style;
        m_width = width;
        m_color = color;
        return TRUE;
    }
    COLORREF GetColor() const { return m_color; }

private:
    int m_style = PS_SOLID;
    int m_width = 1;
    COLORREF m_color = 0;
};

class CDC;

// CBitmap - an RGB32 pixel buffer
class CBitmap {
public:
    CBitmap() = default;

    BOOL CreateCompatibleBitmap(CDC*, int w, int h) { return Create(w, h); }
    BOOL Create(int w, int h)
    {
        if (w <= 0 || h <= 0) return FALSE;
        m_w = w;
        m_h = h;
        m_px.assign((size_t)w * h, 0xFF000000u);
        m_version++;
        return TRUE;
    }
    // Bitmap resource: MAKEINTRESOURCE(IDB_...)
    BOOL LoadBitmap(LPCTSTR name) { return LoadBitmap((UINT)(uintptr_t)name); }
    BOOL LoadBitmap(UINT id)
    {
        const unsigned char* data;
        size_t size;
        return RmtFindResource(id, &data, &size) && LoadBMP(data, size);
    }
    BOOL LoadBMP(const unsigned char* data, size_t size); // Windows .bmp, 1/4/8/24/32 bit
    BOOL DeleteObject()
    {
        m_px.clear();
        m_w = m_h = 0;
        m_version++;
        return TRUE;
    }

    int Width() const { return m_w; }
    int Height() const { return m_h; }
    uint32_t* Bits() { return m_px.empty() ? nullptr : m_px.data(); }
    const uint32_t* Bits() const { return m_px.empty() ? nullptr : m_px.data(); }
    // Changes when the bitmap is created or loaded again (not when it is drawn into)
    unsigned Version() const { return m_version; }

private:
    int m_w = 0, m_h = 0;
    std::vector<uint32_t> m_px;
    unsigned m_version = 0;
};

class CBrush {
public:
    CBrush() = default;
    explicit CBrush(COLORREF c) : m_color(c) {}
    BOOL CreateSolidBrush(COLORREF c)
    {
        m_color = c;
        return TRUE;
    }
    COLORREF GetColor() const { return m_color; }

private:
    COLORREF m_color = 0;
};

// ---------------------------------------------------------------------------
// CDC - software device context drawing into the selected CBitmap: the few
// GDI primitives the GUI-shared sources use (GUI_Song.cpp, GUI_Instruments.cpp,
// GuiHelpers.cpp, TracksControl.cpp, ChannelControl.cpp, RmtView.cpp), with
// GDI semantics: FillSolidRect/FrameRect exclude right/bottom, LineTo excludes
// the end point, BitBlt/StretchBlt only SRCCOPY (the only mode used).
// A window host (Qt) shows the bitmap of its DC.
//
// Between BeginFrame() and EndFrame() the drawing calls are only recorded: the
// tracker draws its whole screen every frame (CRmtView::DrawAll), and
// EndFrame() draws only the tiles of the bitmap whose calls are not the same
// as in the previous frame. ChangedRects() is then what changed.
// ---------------------------------------------------------------------------

class CDC {
public:
    void* m_hDC = nullptr; // non-null once created

    virtual ~CDC() = default;

    BOOL CreateCompatibleDC(CDC*)
    {
        m_hDC = this;
        return TRUE;
    }
    BOOL DeleteDC()
    {
        m_hDC = nullptr;
        m_bitmap = nullptr;
        return TRUE;
    }

    CBitmap* SelectObject(CBitmap* bmp)
    {
        CBitmap* prev = m_bitmap;
        m_bitmap = bmp;
        return prev;
    }
    CPen* SelectObject(CPen* pen)
    {
        CPen* prev = m_pen;
        m_pen = pen;
        return prev;
    }
    CBitmap* GetBitmap() const { return m_bitmap; }

    void FillSolidRect(int x, int y, int cx, int cy, COLORREF color) { Fill(x, y, x + cx, y + cy, RmtPixel(color)); }
    void FillSolidRect(const CRect& r, COLORREF color) { Fill(r.left, r.top, r.right, r.bottom, RmtPixel(color)); }

    void FrameRect(const CRect& r, CBrush* brush)
    {
        uint32_t c = RmtPixel(brush ? brush->GetColor() : 0);
        Fill(r.left, r.top, r.right, r.top + 1, c);
        Fill(r.left, r.bottom - 1, r.right, r.bottom, c);
        Fill(r.left, r.top, r.left + 1, r.bottom, c);
        Fill(r.right - 1, r.top, r.right, r.bottom, c);
    }

    CPoint MoveTo(int x, int y)
    {
        CPoint prev(m_curX, m_curY);
        m_curX = x;
        m_curY = y;
        return prev;
    }
    BOOL LineTo(int x, int y);

    BOOL BitBlt(int x, int y, int w, int h, CDC* src, int xs, int ys, DWORD rop);
    BOOL StretchBlt(int x, int y, int w, int h, CDC* src, int xs, int ys, int ws, int hs, DWORD rop);
    int SetStretchBltMode(int mode)
    {
        int prev = m_stretchMode;
        m_stretchMode = mode;
        return prev;
    }

    void BeginFrame();
    void EndFrame();
    // The parts of the bitmap the last EndFrame() changed
    const std::vector<CRect>& ChangedRects() const { return m_changed; }

private:
    struct DrawOp {
        enum Kind : uint8_t { FILL, BLIT, LINE } kind;
        int l, t, r, b;           // FILL: the rectangle; BLIT: the destination; LINE: from (l,t) to (r,b), without the end point
        uint32_t color;           // FILL, LINE
        const CBitmap* src;       // BLIT
        unsigned srcVersion;      // BLIT
        int xs, ys, ws, hs;       // BLIT: the source rectangle
        CRect Bounds() const;
        uint64_t Hash() const;
    };
    static constexpr int TILE_W = 32, TILE_H = 16;

    // the drawing calls: recorded in a frame, else drawn at once
    void Fill(int l, int t, int r, int b, uint32_t c);
    BOOL Blit(int x, int y, int w, int h, CDC* src, int xs, int ys, int ws, int hs);
    // the pixels, within clip (in the bitmap); a line only in the tiles of dirtyTiles (nullptr: everywhere)
    void FillPixels(int l, int t, int r, int b, uint32_t c, const CRect& clip);
    void BlitPixels(int x, int y, int w, int h, const CBitmap* sb, int xs, int ys, int ws, int hs, const CRect& clip);
    void LinePixels(int x0, int y0, int x1, int y1, uint32_t c, const std::vector<char>* dirtyTiles);
    void DrawRecorded(const DrawOp& op, const CRect& clip);
    CRect BitmapRect() const { return CRect(0, 0, m_bitmap->Width(), m_bitmap->Height()); }

    CBitmap* m_bitmap = nullptr;
    CPen* m_pen = nullptr;
    int m_curX = 0, m_curY = 0;
    int m_stretchMode = COLORONCOLOR;

    bool m_recording = false;
    bool m_frameFull = false;  // this frame is drawn whole (a blit from its own bitmap)
    bool m_drawnOutside = true; // drawn into outside a frame (or never drawn): the next frame is drawn whole
    std::vector<DrawOp> m_ops;
    std::vector<uint64_t> m_tileHash; // of the calls that drew each tile in the previous frame
    std::vector<uint64_t> m_newTileHash;
    std::vector<char> m_dirty;
    int m_tilesX = 0, m_tilesY = 0;
    const CBitmap* m_frameBitmap = nullptr;
    unsigned m_frameBitmapVersion = 0;
    std::vector<CRect> m_changed;
};

// ---------------------------------------------------------------------------
// CWnd / CFrameWnd / simple control shims - just enough for MainFrm.h /
// EffectsDlg.h / importdlgs.h / exportdlgs.h / filenewdlg.h to declare
// their member variables, and for the (dead, headless) code paths that
// cast AfxGetMainWnd()/AfxGetApp()->GetMainWnd() to keep compiling.
// ---------------------------------------------------------------------------

struct CDataExchange;
struct tagMSG;
typedef struct tagMSG MSG; // as <windows.h> and qcoreapplication.h
struct CREATESTRUCT;
typedef CREATESTRUCT* LPCREATESTRUCT;
struct MINMAXINFO;

class CWnd;
class CDialog;
class CControlBar;
class CFrameWnd;
typedef intptr_t LRESULT;

// ---------------------------------------------------------------------------
// IRmtHost - what the window classes ask of the window system. The GUI
// frontend (src/qt) implements it and sets g_rmtHost; with no host (headless
// RmtCoreTest) the calls do nothing and MessageBox prints to stderr.
// ---------------------------------------------------------------------------

struct IRmtHost {
    virtual ~IRmtHost() = default;
    virtual CFrameWnd* GetMainWnd() = 0;
    virtual void GetClientRect(const CWnd* wnd, RECT* r) = 0;
    virtual void Invalidate(CWnd* wnd) = 0;
    virtual CDC* GetDC(CWnd* wnd) = 0;
    virtual UINT_PTR SetTimer(CWnd* wnd, UINT_PTR id, UINT ms) = 0;
    virtual BOOL KillTimer(CWnd* wnd, UINT_PTR id) = 0;
    virtual void PostCommand(UINT id) = 0; // WM_COMMAND
    virtual void Close() = 0;              // WM_CLOSE
    virtual int MessageBox(const char* text, const char* caption, UINT type) = 0;
    virtual HCURSOR LoadCursor(UINT id) = 0; // IDC_* of resource.h, or system IDC_ARROW/IDC_WAIT
    virtual void SetCursor(HCURSOR cursor) = 0;
    virtual short GetKeyState(int vk) = 0;
    virtual UINT MapVirtualKeyToChar(UINT vk) = 0;
    virtual void SetWindowText(CWnd* wnd, const char* text) = 0;
    virtual void ShowControlBar(CControlBar* bar, BOOL show) = 0;
    virtual BOOL IsControlBarVisible(const CControlBar* bar) = 0;
    virtual void SetStatusText(int pane, const char* text) = 0;
    // Common Open/Save dialog (CFileDialog::DoModal). filter is in MFC format
    // ("Name|*.a;*.b|...||"), filterIndex is 1-based in and out. Returns false
    // when cancelled.
    virtual bool FileDialog(bool open, const char* title, const char* initialDir, const char* fileName,
                            const char* filter, DWORD flags, int& filterIndex, CString& path) = 0;
    // CDialog::DoModal: the frontend shows the dialog of dlg->m_nIDTemplate
    // (IDD_* of resource.h), reads and writes the dialog's data members and
    // returns IDOK / IDCANCEL; dialogs it does not know are cancelled.
    virtual INT_PTR DoModal(CDialog* dlg) = 0;
};
extern IRmtHost* g_rmtHost;

// ---------------------------------------------------------------------------
// CCmdTarget and the message maps. BEGIN_MESSAGE_MAP ... END_MESSAGE_MAP
// build a table of the ON_COMMAND / ON_UPDATE_COMMAND_UI handlers (as MFC
// does), used by the frontend to run menu, toolbar and accelerator commands
// and to update their state. Window messages (ON_WM_*) are delivered by the
// frontend calling the handlers directly, so those entries are empty.
// ---------------------------------------------------------------------------

class CCmdUI {
public:
    UINT m_nID = 0;
    BOOL m_bEnabled = TRUE;
    int m_nCheck = 0;
    CString m_strText;
    bool m_bTextSet = false;

    virtual ~CCmdUI() = default;
    virtual void Enable(BOOL on = TRUE) { m_bEnabled = on; }
    virtual void SetCheck(int check = 1) { m_nCheck = check; }
    virtual void SetRadio(BOOL on = TRUE) { m_nCheck = on ? 1 : 0; }
    virtual void SetText(LPCTSTR text)
    {
        m_strText = text;
        m_bTextSet = true;
    }
};

class CCmdTarget;
typedef void (CCmdTarget::*RmtCmdFn)();
typedef void (CCmdTarget::*RmtUpdFn)(CCmdUI*);
struct RmtMsgEntry {
    UINT id;
    RmtCmdFn cmd; // ON_COMMAND
    RmtUpdFn upd; // ON_UPDATE_COMMAND_UI
};

class CCmdTarget {
public:
    virtual ~CCmdTarget() = default;
    virtual const RmtMsgEntry* GetMessageEntries() const { return nullptr; }

    // run the ON_COMMAND handler of id; FALSE if there is none
    BOOL OnCmdMsg(UINT id)
    {
        for (const RmtMsgEntry* e = GetMessageEntries(); e && e->id; e++)
            if (e->id == id && e->cmd) {
                (this->*(e->cmd))();
                return TRUE;
            }
        return FALSE;
    }
    // run the ON_UPDATE_COMMAND_UI handler of ui->m_nID; FALSE if there is none
    BOOL OnUpdateCmdUI(CCmdUI* ui)
    {
        for (const RmtMsgEntry* e = GetMessageEntries(); e && e->id; e++)
            if (e->id == ui->m_nID && e->upd) {
                (this->*(e->upd))(ui);
                return TRUE;
            }
        return FALSE;
    }
    BOOL HasCommand(UINT id) const
    {
        for (const RmtMsgEntry* e = GetMessageEntries(); e && e->id; e++)
            if (e->id == id && e->cmd) return TRUE;
        return FALSE;
    }
};

#define DECLARE_MESSAGE_MAP() \
public:                       \
    const RmtMsgEntry* GetMessageEntries() const override;
#define BEGIN_MESSAGE_MAP(theClass, baseClass)             \
    const RmtMsgEntry* theClass::GetMessageEntries() const \
    {                                                      \
        using ThisClass [[maybe_unused]] = theClass;       \
        static const RmtMsgEntry entries[] = {
#define END_MESSAGE_MAP()   \
    {                       \
        0, nullptr, nullptr \
    }                       \
    }                       \
    ;                       \
    return entries;         \
    }
#define ON_COMMAND(id, fn)           { (UINT)(id), static_cast<RmtCmdFn>(&ThisClass::fn), nullptr },
#define ON_UPDATE_COMMAND_UI(id, fn) { (UINT)(id), nullptr, static_cast<RmtUpdFn>(&ThisClass::fn) },
#define ON_CBN_SELCHANGE(id, fn)
#define ON_CBN_CLOSEUP(id, fn)
#define ON_CBN_KILLFOCUS(id, fn)
#define ON_BN_CLICKED(id, fn)
#define ON_EN_CHANGE(id, fn)
#define ON_WM_CLOSE()
#define ON_WM_CREATE()
#define ON_WM_DESTROY()
#define ON_WM_ERASEBKGND()
#define ON_WM_GETMINMAXINFO()
#define ON_WM_KEYDOWN()
#define ON_WM_KEYUP()
#define ON_WM_KILLFOCUS()
#define ON_WM_LBUTTONDBLCLK()
#define ON_WM_LBUTTONDOWN()
#define ON_WM_LBUTTONUP()
#define ON_WM_MOUSEMOVE()
#define ON_WM_MOUSEWHEEL()
#define ON_WM_RBUTTONDBLCLK()
#define ON_WM_RBUTTONDOWN()
#define ON_WM_RBUTTONUP()
#define ON_WM_SETCURSOR()
#define ON_WM_SETFOCUS()
#define ON_WM_SIZE()
#define ON_WM_SYSCHAR()
#define ON_WM_TIMER()

// ---------------------------------------------------------------------------
// CWnd and friends: window operations go to g_rmtHost; the ON_WM_* handler
// defaults do nothing (derived classes call CView::OnTimer() etc.)
// ---------------------------------------------------------------------------

class CWnd : public CCmdTarget {
public:
    HWND m_hWnd = nullptr;
    virtual ~CWnd() = default;

    void GetClientRect(LPRECT r) const
    {
        if (g_rmtHost)
            g_rmtHost->GetClientRect(this, r);
        else
            *r = RECT{ 0, 0, 0, 0 };
    }
    void Invalidate(BOOL = TRUE)
    {
        if (g_rmtHost) g_rmtHost->Invalidate(this);
    }
    void UpdateWindow() {}
    CDC* GetDC() { return g_rmtHost ? g_rmtHost->GetDC(this) : nullptr; }
    int ReleaseDC(CDC*) { return 1; }
    UINT_PTR SetTimer(UINT_PTR id, UINT ms, void*) { return g_rmtHost ? g_rmtHost->SetTimer(this, id, ms) : 0; }
    BOOL KillTimer(UINT_PTR id) { return g_rmtHost ? g_rmtHost->KillTimer(this, id) : FALSE; }
    BOOL PostMessage(UINT msg, WPARAM wParam = 0, LPARAM = 0)
    {
        if (!g_rmtHost) return FALSE;
        if (msg == WM_COMMAND)
            g_rmtHost->PostCommand((UINT)(wParam & 0xFFFF));
        else if (msg == WM_CLOSE)
            g_rmtHost->Close();
        return TRUE;
    }
    LRESULT SendMessage(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) { return PostMessage(msg, wParam, lParam); }
    int MessageBox(LPCTSTR text, LPCTSTR caption = nullptr, UINT type = 0);
    void SetWindowText(LPCTSTR text)
    {
        if (g_rmtHost) g_rmtHost->SetWindowText(this, text);
    }
    CWnd* SetFocus() { return this; }
    BOOL GetSafeHwnd() const { return m_hWnd != nullptr; }
    BOOL IsWindowVisible() const { return TRUE; }

    void OnDestroy() {}
    void OnTimer(UINT_PTR) {}
    void OnSize(UINT, int, int) {}
    void OnKeyDown(UINT, UINT, UINT) {}
    void OnKeyUp(UINT, UINT, UINT) {}
    void OnSysChar(UINT, UINT, UINT) {}
    void OnLButtonDown(UINT, CPoint) {}
    void OnLButtonUp(UINT, CPoint) {}
    void OnLButtonDblClk(UINT, CPoint) {}
    void OnRButtonDown(UINT, CPoint) {}
    void OnRButtonUp(UINT, CPoint) {}
    void OnRButtonDblClk(UINT, CPoint) {}
    void OnMouseMove(UINT, CPoint) {}
    BOOL OnMouseWheel(UINT, short, CPoint) { return TRUE; }
    BOOL OnSetCursor(CWnd*, UINT, UINT) { return TRUE; }
    void OnSetFocus(CWnd*) {}
    void OnKillFocus(CWnd*) {}
    BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    BOOL PreCreateWindow(CREATESTRUCT&) { return TRUE; }
};

class CControlBar : public CWnd {};
class CToolBar : public CControlBar {};
class CReBar : public CControlBar {};
class CStatusBar : public CControlBar {
public:
    BOOL SetPaneText(int pane, LPCTSTR text, BOOL = TRUE)
    {
        if (g_rmtHost) g_rmtHost->SetStatusText(pane, text);
        return TRUE;
    }
};

class CFrameWnd : public CWnd {
public:
    virtual ~CFrameWnd() = default;
    void ShowControlBar(CControlBar* bar, BOOL show, BOOL /*delay*/)
    {
        if (g_rmtHost) g_rmtHost->ShowControlBar(bar, show);
    }
};

class CDocument : public CCmdTarget {
public:
    virtual ~CDocument() = default;
    virtual BOOL OnNewDocument() { return TRUE; }
    void SetTitle(LPCTSTR title) { m_strTitle = title; }
    const CString& GetTitle() const { return m_strTitle; }
    void SetModifiedFlag(BOOL modified = TRUE) { m_bModified = modified; }
    BOOL IsModified() const { return m_bModified; }
    void UpdateAllViews(void*, LPARAM = 0, void* = nullptr)
    {
        if (g_rmtHost) g_rmtHost->Invalidate(nullptr);
    }

private:
    CString m_strTitle;
    BOOL m_bModified = FALSE;
};

class CArchive {};
struct CPrintInfo {};

class CView : public CWnd {
public:
    CDocument* m_pDocument = nullptr;
    virtual ~CView() = default;
    CDocument* GetDocument() const { return m_pDocument; }
    virtual void OnInitialUpdate() {}
    virtual void OnDraw(CDC*) {}
    void OnFilePrint() {}
    void OnFilePrintPreview() {}
    BOOL OnPreparePrinting(CPrintInfo*) { return FALSE; }
    BOOL DoPreparePrinting(CPrintInfo*) { return FALSE; }
    void OnBeginPrinting(CDC*, CPrintInfo*) {}
    void OnEndPrinting(CDC*, CPrintInfo*) {}
};

class CStatic : public CWnd {};
class CEdit : public CWnd {};
class CButton : public CWnd {};
class CListBox : public CWnd {
public:
    void ResetContent() {}
    int AddString(const char*) { return 0; }
    void SetCurSel(int) {}
    int GetCurSel() const { return -1; }
};
class CComboBox : public CWnd {
public:
    void ResetContent() {}
    int AddString(const char*) { return 0; }
    void SetCurSel(int) {}
    int GetCurSel() const { return -1; }
};
class CScrollBar : public CWnd {};
class CFont : public CWnd {};

// CDialog - DoModal() asks the frontend (IRmtHost::DoModal), which shows the
// dialog of m_nIDTemplate if it has one; otherwise, and with no frontend
// (headless RmtCoreTest), it reports "cancelled", the code path every call
// site already has to handle for the "user dismissed the dialog" case.
class CDialog : public CWnd {
public:
    CDialog() = default;
    explicit CDialog(UINT nIDTemplate, CWnd* /*pParentWnd*/ = nullptr) : m_nIDTemplate(nIDTemplate) {}
    virtual ~CDialog() = default;

    UINT m_nIDTemplate = 0; // IDD_* of the dialog

    virtual INT_PTR DoModal();
    virtual void DoDataExchange(CDataExchange*) {}
    virtual BOOL OnInitDialog() { return TRUE; }
    virtual void OnOK() {}
    virtual void OnCancel() {}
    virtual BOOL PreTranslateMessage(MSG*) { return FALSE; }
};

// ---------------------------------------------------------------------------
// CFileDialog - the common Open/Save dialog, used directly in IO_Song.cpp.
// DoModal() asks the frontend (IRmtHost::FileDialog); with no frontend
// (headless RmtCoreTest) it "cancels". m_ofn carries the title, initial
// folder and file name in, and the chosen filter index out, as with MFC.
// ---------------------------------------------------------------------------

struct OPENFILENAME_STUB {
    const char* lpstrTitle = nullptr;
    const char* lpstrInitialDir = nullptr;
    char* lpstrFile = nullptr;
    UINT nMaxFile = 0;
    int nFilterIndex = 0;
};

#define OFN_HIDEREADONLY    0x00000004L
#define OFN_OVERWRITEPROMPT 0x00000002L
#define OFN_FILEMUSTEXIST   0x00001000L

class CFileDialog : public CDialog {
public:
    CFileDialog(BOOL bOpenFileDialog,
                LPCTSTR lpszDefExt = nullptr,
                LPCTSTR lpszFileName = nullptr,
                DWORD dwFlags = 0,
                LPCTSTR lpszFilter = nullptr,
                CWnd* pParentWnd = nullptr)
        : m_bOpen(bOpenFileDialog), m_fileName(lpszFileName), m_flags(dwFlags), m_filter(lpszFilter)
    {
        (void)lpszDefExt;
        (void)pParentWnd;
    }

    INT_PTR DoModal() override;
    CString GetPathName() const { return m_path; }
    CString GetFileName() const
    {
        int pos = m_path.ReverseFind('/');
        if (pos < 0) pos = m_path.ReverseFind('\\');
        return pos < 0 ? m_path : m_path.Mid(pos + 1);
    }

    OPENFILENAME_STUB m_ofn;

private:
    BOOL m_bOpen = TRUE;
    CString m_fileName;
    DWORD m_flags = 0;
    CString m_filter;
    CString m_path;
};

// ---------------------------------------------------------------------------
// CFile / CFileStatus / CByteArray - small but genuinely functional helpers
// (backed by std::filesystem / std::fstream), used by AtariBinaries.cpp and
// SongExporterTest.cpp.
// ---------------------------------------------------------------------------

struct CFileStatus {
    long m_size = 0;
};

class CFile {
public:
    enum OpenFlags { modeRead = 0,
                     modeWrite = 1,
                     modeReadWrite = 2,
                     modeCreate = 0x1000 };

    CFile() = default;
    CFile(const char* path, UINT mode) : m_path(path ? path : "")
    {
        auto flags = (mode & modeWrite) ? (std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc)
                                        : (std::ios::in | std::ios::binary);
        m_stream.open(m_path, flags);
    }
    ~CFile() { Close(); }

    static BOOL GetStatus(const char* path, CFileStatus& status)
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(path ? path : "", ec);
        if (ec) return FALSE;
        status.m_size = (long)sz;
        return TRUE;
    }
    static BOOL Remove(const char* path)
    {
        std::error_code ec;
        return std::filesystem::remove(path ? path : "", ec) ? TRUE : FALSE;
    }

    UINT Read(void* buffer, UINT count)
    {
        if (!m_stream.is_open()) return 0;
        m_stream.read((char*)buffer, count);
        return (UINT)m_stream.gcount();
    }

    long GetLength() const
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(m_path, ec);
        return ec ? 0 : (long)sz;
    }

    CString GetFileName() const
    {
        std::filesystem::path p(m_path);
        return CString(p.filename().string());
    }

    void Close()
    {
        if (m_stream.is_open()) m_stream.close();
    }

private:
    std::string m_path;
    std::fstream m_stream;
};

class CByteArray {
public:
    void SetSize(long size) { m_data.resize(size > 0 ? (size_t)size : 0); }
    byte* GetData() { return m_data.empty() ? nullptr : m_data.data(); }
    long GetSize() const { return (long)m_data.size(); }

private:
    std::vector<byte> m_data;
};

// ---------------------------------------------------------------------------
// CWinApp / AfxGetApp / AfxGetMainWnd / AfxMessageBox
//
// GetMainWnd() intentionally returns nullptr: several call sites
// (GUI_Song.cpp CSong::DrawInfo, GuiHelpers.cpp) were already written with
// a "headless mode" null-check for exactly this situation.
// ---------------------------------------------------------------------------

class CWinApp {
public:
    HINSTANCE m_hInstance = nullptr;
    CWnd* GetMainWnd() { return g_rmtHost ? (CWnd*)g_rmtHost->GetMainWnd() : nullptr; }
};

inline CWinApp* AfxGetApp()
{
    static CWinApp s_app;
    return &s_app;
}
inline CWnd* AfxGetMainWnd() { return g_rmtHost ? (CWnd*)g_rmtHost->GetMainWnd() : nullptr; }
inline int AfxMessageBox(const char* text, UINT type = MB_OK, UINT = 0)
{
    if (g_rmtHost) return g_rmtHost->MessageBox(text, "RITMO", type);
    std::fprintf(stderr, "[AfxMessageBox] %s\n", text ? text : "");
    (void)type;
    return IDOK;
}

// The application object: the version string and the web pages of the help.
class CRmtApp {
public:
    CString GetVersionAndBuild() const;
    void OpenOnlineHelp(); // frontend (opens the help URL)
    void OpenUrl(const char* url); // frontend (opens a web page in the browser)
};
extern CRmtApp g_app;

// ---------------------------------------------------------------------------
// MessageBox / Shell / misc Win32 free functions used directly by the
// engine. All are safe no-ops or best-effort equivalents; none of them is
// reachable from the RmtCoreTest smoke-test main(), so their exact
// behaviour is not runtime-critical for this build.
// ---------------------------------------------------------------------------

inline int MessageBox(HWND, const char* text, const char* caption, UINT type)
{
    if (g_rmtHost) return g_rmtHost->MessageBox(text, caption, type);
    std::fprintf(stderr, "[MessageBox] %s: %s\n", caption ? caption : "", text ? text : "");
    return (type & 0x0F) == MB_YESNO || (type & 0x0F) == MB_YESNOCANCEL ? IDYES : IDOK;
}
inline int CWnd::MessageBox(LPCTSTR text, LPCTSTR caption, UINT type) { return ::MessageBox(m_hWnd, text, caption, type); }

inline void GetWindowRect(HWND, CRect* rect)
{
    if (rect) *rect = CRect(0, 0, 0, 0);
}


inline DWORD GetLastError() { return 0; }
inline DWORD FormatMessage(DWORD, LPCVOID, DWORD, DWORD, LPTSTR, DWORD, void*) { return 0; }
inline void LocalFree(void*) {}
inline HANDLE ShellExecute(HWND, const char*, const char*, const char*, const char*, int)
{
    // Always report "success" (handle value > 32) so callers never take the
    // Win32-error-reporting branch (FormatMessage/GetLastError) which is not
    // meaningfully implementable here.
    return (HANDLE)(intptr_t)33;
}
#define SW_SHOWNORMAL                  1
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define MAKELANGID(p, s)               0
#define LANG_NEUTRAL                   0
#define SUBLANG_DEFAULT                0

inline void ZeroMemory(void* dst, size_t size)
{
    std::memset(dst, 0, size);
}

// Sleep() / GetTickCount()
void Sleep(DWORD ms);
DWORD GetTickCount(); // milliseconds, wraps like the Windows one

// Windows MIDI API: only the device enumeration used by the configuration
// code of RmtView.cpp, over the RtMidi input ports (RmtMidiRt.cpp, which is
// also the MIDI IN of CRmtMidi). szPname is longer than the 32 characters of
// Windows: ALSA port names often are.
struct MIDIINCAPS {
    WORD wMid;
    WORD wPid;
    UINT vDriverVersion;
    char szPname[256];
    DWORD dwSupport;
};
#define MMSYSERR_NOERROR     0
#define MMSYSERR_BADDEVICEID 2
UINT midiInGetNumDevs();
UINT midiInGetDevCaps(UINT_PTR id, MIDIINCAPS* caps, UINT size);

#define LOWORD(l) ((WORD)((DWORD_PTR)(l)&0xffff))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xffff))

#define IDC_ARROW ((LPCTSTR)(intptr_t)32512)
#define IDC_WAIT  ((LPCTSTR)(intptr_t)32514)
inline HCURSOR LoadCursor(HINSTANCE, LPCTSTR id)
{
    return g_rmtHost ? g_rmtHost->LoadCursor((UINT)(uintptr_t)id) : nullptr;
}
inline HCURSOR SetCursor(HCURSOR c)
{
    if (g_rmtHost) g_rmtHost->SetCursor(c);
    return nullptr;
}
inline BOOL EnableWindow(HWND, BOOL) { return TRUE; }
inline BOOL UpdateWindow(HWND) { return TRUE; }
inline short GetKeyState(int vk) { return g_rmtHost ? g_rmtHost->GetKeyState(vk) : 0; }
inline short GetAsyncKeyState(int vk) { return GetKeyState(vk); }

#define MAPVK_VK_TO_CHAR 2
inline UINT MapVirtualKeyEx(UINT vk, UINT type, HANDLE)
{
    return g_rmtHost && type == MAPVK_VK_TO_CHAR ? g_rmtHost->MapVirtualKeyToChar(vk) : 0;
}

inline void OutputDebugString(const char* s) { std::fprintf(stderr, "%s", s ? s : ""); }

inline int _strcmpi(const char* a, const char* b) { return strcasecmp(a, b); }

inline BOOL DeleteFile(const char* path)
{
    std::error_code ec;
    return std::filesystem::remove(path ? path : "", ec) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// CTime - tiny wrapper around time_t/strftime, used by SongExporter
// (records the export timestamp and formats it for display).
// ---------------------------------------------------------------------------

class CTime {
public:
    CTime() = default;
    explicit CTime(std::time_t t) : m_time(t) {}

    static CTime GetCurrentTime() { return CTime(std::time(nullptr)); }

    CString Format(const char* fmt) const
    {
        char buf[128];
        std::tm tmVal{};
#if defined(_WIN32)
        localtime_s(&tmVal, &m_time);
#else
        localtime_r(&m_time, &tmVal);
#endif
        std::strftime(buf, sizeof(buf), fmt, &tmVal);
        return CString(buf);
    }

private:
    std::time_t m_time = 0;
};

// ---------------------------------------------------------------------------
// Legacy multimedia timer (winmm) - used only by SongTimer.cpp. Never
// actually starts a background callback here (headless build); the engine
// already guards all playback state via g_closeApplication/g_rmtroutine so
// simply never firing the callback is safe for a non-interactive build.
// ---------------------------------------------------------------------------

typedef void(CALLBACK* LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
#define TIME_PERIODIC 1
#define TIME_ONESHOT  0
// multimedia timers on a thread (CompatAudio.cpp)
UINT timeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags);
UINT timeKillEvent(UINT id);

// ---------------------------------------------------------------------------
// Minimal DirectSound shim (legacy CXPokey/PokeyRenderer.cpp renderer).
// DirectSound types; the device below is silent (see IDirectSoundBuffer).
// ---------------------------------------------------------------------------

struct WAVEFORMATEX {
    WORD wFormatTag = 0;
    WORD nChannels = 0;
    DWORD nSamplesPerSec = 0;
    DWORD nAvgBytesPerSec = 0;
    WORD nBlockAlign = 0;
    WORD wBitsPerSample = 0;
    WORD cbSize = 0;
};
#define WAVE_FORMAT_PCM 1

struct DSBUFFERDESC {
    DWORD dwSize = 0;
    DWORD dwFlags = 0;
    DWORD dwBufferBytes = 0;
    DWORD dwReserved = 0;
    WAVEFORMATEX* lpwfxFormat = nullptr;
};
struct DSBCAPS {
    DWORD dwSize = 0;
    DWORD dwFlags = 0;
    DWORD dwBufferBytes = 0;
};

#define DS_OK                       0L
#define DS_ERR_GENERIC              (-1L)
#define DSSCL_PRIORITY              2
#define DSBCAPS_PRIMARYBUFFER       1
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBCAPS_LOCHARDWARE         0x00000004
#define DSBCAPS_LOCSOFTWARE         0x00000008
#define DSBCAPS_GLOBALFOCUS         0x00008000
#define DSBCAPS_STICKYFOCUS         0x00004000
#define DSBLOCK_FROMWRITECURSOR     0x00000001
#define DSBPLAY_LOOPING             0x00000001

// DirectSound outside Windows (CompatAudio.cpp): a secondary buffer is a ring
// that PortAudio plays (when available and g_rmtAudioOutput is true), with
// real play/write cursors, so PokeyRenderer.cpp streams to it exactly as to
// DirectSound. Otherwise the buffer "plays" instantly: the cursors follow the
// writes (RmtCoreTest, builds without PortAudio).
extern bool g_rmtAudioOutput;

struct RmtAudioStream;
class IDirectSoundBuffer {
public:
    IDirectSoundBuffer(DWORD bytes, const WAVEFORMATEX* format);
    ~IDirectSoundBuffer();
    HRESULT SetFormat(const WAVEFORMATEX* format)
    {
        if (format) m_format = *format;
        return DS_OK;
    }
    HRESULT GetCaps(DSBCAPS* caps)
    {
        if (caps) caps->dwBufferBytes = (DWORD)m_data.size();
        return DS_OK;
    }
    HRESULT Lock(DWORD offset, DWORD bytes, void** ppData1, DWORD* pSize1, void** ppData2, DWORD* pSize2, DWORD)
    {
        DWORD size = (DWORD)m_data.size();
        if (ppData2) *ppData2 = nullptr;
        if (pSize2) *pSize2 = 0;
        if (!size || !ppData1 || !pSize1) return DS_ERR_GENERIC;
        offset %= size;
        if (bytes > size) bytes = size;
        DWORD first = std::min(bytes, size - offset);
        *ppData1 = m_data.data() + offset;
        *pSize1 = first;
        if (bytes > first && ppData2 && pSize2) {
            *ppData2 = m_data.data();
            *pSize2 = bytes - first;
        }
        return DS_OK;
    }
    HRESULT Unlock(void* p1, DWORD s1, void* p2, DWORD s2);
    HRESULT Play(DWORD, DWORD, DWORD flags);
    HRESULT Stop();
    HRESULT GetCurrentPosition(DWORD* playCursor, DWORD* writeCursor);
    HRESULT Release()
    {
        delete this;
        return DS_OK;
    }

    // PortAudio side
    std::vector<unsigned char> m_data;
    WAVEFORMATEX m_format;
    std::atomic<DWORD> m_play{ 0 }; // byte offset the device reads next
    DWORD m_cursor = 0;             // instant mode: end of the last write
private:
    RmtAudioStream* m_stream = nullptr;
};
typedef IDirectSoundBuffer* LPDIRECTSOUNDBUFFER;

class IDirectSound {
public:
    HRESULT SetCooperativeLevel(HWND, DWORD) { return DS_OK; }
    HRESULT CreateSoundBuffer(const DSBUFFERDESC* desc, LPDIRECTSOUNDBUFFER* buffer, void*)
    {
        if (!buffer) return DS_ERR_GENERIC;
        *buffer = new IDirectSoundBuffer(desc ? desc->dwBufferBytes : 0, desc ? desc->lpwfxFormat : nullptr);
        return DS_OK;
    }
    HRESULT Release() { return DS_OK; }
};
typedef IDirectSound* LPDIRECTSOUND;

inline HRESULT DirectSoundCreate(void*, LPDIRECTSOUND* ds, void*)
{
    static IDirectSound device;
    if (!ds) return DS_ERR_GENERIC;
    *ds = &device;
    return DS_OK;
}
