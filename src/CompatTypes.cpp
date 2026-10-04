// CompatTypes.cpp
//
// Out-of-line pieces of CompatTypes.h

#include "PlatformTypes.h"
#include "resource.h"
#include "RmtVersion.h"

#include <algorithm>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
// CString::LoadString() - stand-in for the Windows .rc string table.
// Only the resource IDs actually reached via LoadString() calls in the
// engine + GUI-shared source files are listed here (verified by grep):
//   IDS_RMTVERSION / IDS_RMT_VERSION (IO_Song.cpp)
// IDS_RMT_AUTHOR and IDS_RMT_REPOSITORY are kept here too, they cost nothing
// and keep the table self-documenting. Text taken verbatim from Rmt.rc.
// ---------------------------------------------------------------------------

BOOL CString::LoadString(UINT id)
{
    switch (id) {
        case IDS_RMT_VERSION: // == IDS_RMTVERSION
            m_data = RMT_VERSION_STRING;
            return TRUE;
        case IDS_RMT_AUTHOR:
            m_data = "based on RASTER Music Tracker by Radek Sterba (R.I.P.), (c) Raster/C.P.U. (2002-2009), VinsCool (2021-2024), JAC! (2024-2026)";
            return TRUE;
        case IDS_RMT_REPOSITORY:
            m_data = "https://github.com/raster-atari-org/RASTER-Music-Tracker";
            return TRUE;
        default:
            m_data = "";
            return FALSE;
    }
}

// ---------------------------------------------------------------------------
// CRmtApp - the application object: the version string displayed by
// GUI_Song.cpp.
// ---------------------------------------------------------------------------

CRmtApp g_app;

IRmtHost* g_rmtHost = nullptr;

INT_PTR CDialog::DoModal()
{
    return g_rmtHost ? g_rmtHost->DoModal(this) : IDCANCEL;
}

INT_PTR CFileDialog::DoModal()
{
    if (!g_rmtHost) return IDCANCEL;
    const char* fileName = m_ofn.lpstrFile && *m_ofn.lpstrFile ? m_ofn.lpstrFile : m_fileName.GetString();
    int filterIndex = m_ofn.nFilterIndex;
    CString path;
    if (!g_rmtHost->FileDialog(m_bOpen != FALSE, m_ofn.lpstrTitle, m_ofn.lpstrInitialDir, fileName,
                               m_filter.GetString(), m_flags, filterIndex, path))
        return IDCANCEL;
    m_path = path;
    m_ofn.nFilterIndex = filterIndex;
    return IDOK;
}

CString CRmtApp::GetVersionAndBuild() const
{
    CString version;
    version.LoadString(IDS_RMTVERSION);
    CString result;
    result.Format("%s (%s %s)", version, __DATE__, __TIME__);
    return result;
}

// ---------------------------------------------------------------------------
// CBitmap / CDC - software drawing (see CompatTypes.h)
// ---------------------------------------------------------------------------

static uint32_t rd32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Windows .bmp (BITMAPFILEHEADER + BITMAPINFOHEADER), uncompressed,
// 1/4/8 bit with palette, 24/32 bit; bottom-up or top-down
BOOL CBitmap::LoadBMP(const unsigned char* d, size_t size)
{
    if (!d || size < 54 || d[0] != 'B' || d[1] != 'M') return FALSE;
    uint32_t bits = rd32(d + 10), hdr = rd32(d + 14);
    int w = (int)rd32(d + 18), h = (int)rd32(d + 22);
    int bpp = rd16(d + 28);
    uint32_t comp = rd32(d + 30), used = rd32(d + 46);
    if (comp != 0 || w <= 0 || h == 0 || hdr < 40) return FALSE;
    bool topdown = h < 0;
    if (topdown) h = -h;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) return FALSE;
    uint32_t ncol = bpp <= 8 ? (used ? used : 1u << bpp) : 0;
    const unsigned char* pal = d + 14 + hdr;
    size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
    if (bits + stride * h > size || 14 + hdr + ncol * 4 > size) return FALSE;
    Create(w, h);
    for (int y = 0; y < h; y++) {
        const unsigned char* row = d + bits + stride * (topdown ? y : h - 1 - y);
        uint32_t* out = m_px.data() + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint32_t idx;
            switch (bpp) {
                case 1: idx = (row[x >> 3] >> (7 - (x & 7))) & 1; break;
                case 4: idx = (row[x >> 1] >> ((x & 1) ? 0 : 4)) & 15; break;
                case 8: idx = row[x]; break;
                case 24: out[x] = 0xFF000000u | (row[x * 3 + 2] << 16) | (row[x * 3 + 1] << 8) | row[x * 3]; continue;
                default: out[x] = 0xFF000000u | (row[x * 4 + 2] << 16) | (row[x * 4 + 1] << 8) | row[x * 4]; continue;
            }
            const unsigned char* c = pal + 4 * (idx < ncol ? idx : 0);
            out[x] = 0xFF000000u | (c[2] << 16) | (c[1] << 8) | c[0];
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// CDC drawing
// ---------------------------------------------------------------------------

static uint64_t HashMix(uint64_t h, uint64_t v)
{
    h = (h ^ v) * 0x9E3779B97F4A7C15ull;
    return h ^ (h >> 29);
}

CRect CDC::DrawOp::Bounds() const
{
    if (kind == LINE) return CRect(std::min(l, r), std::min(t, b), std::max(l, r) + 1, std::max(t, b) + 1);
    return CRect(l, t, r, b);
}

uint64_t CDC::DrawOp::Hash() const
{
    uint64_t h = HashMix(kind + 1, ((uint64_t)(uint32_t)l << 32) | (uint32_t)t);
    h = HashMix(h, ((uint64_t)(uint32_t)r << 32) | (uint32_t)b);
    if (kind == BLIT) {
        h = HashMix(h, (uint64_t)(uintptr_t)src);
        h = HashMix(h, srcVersion);
        h = HashMix(h, ((uint64_t)(uint32_t)xs << 32) | (uint32_t)ys);
        h = HashMix(h, ((uint64_t)(uint32_t)ws << 32) | (uint32_t)hs);
    } else
        h = HashMix(h, color);
    return h;
}

void CDC::Fill(int l, int t, int r, int b, uint32_t c)
{
    if (!m_bitmap || !m_bitmap->Bits()) return;
    if (m_recording) {
        DrawOp op{};
        op.kind = DrawOp::FILL;
        op.l = l, op.t = t, op.r = r, op.b = b;
        op.color = c;
        m_ops.push_back(op);
        return;
    }
    m_drawnOutside = true;
    FillPixels(l, t, r, b, c, BitmapRect());
}

void CDC::FillPixels(int l, int t, int r, int b, uint32_t c, const CRect& clip)
{
    l = std::max(l, (int)clip.left);
    t = std::max(t, (int)clip.top);
    r = std::min(r, (int)clip.right);
    b = std::min(b, (int)clip.bottom);
    if (l >= r) return;
    for (int y = t; y < b; y++) {
        uint32_t* p = m_bitmap->Bits() + (size_t)y * m_bitmap->Width();
        std::fill(p + l, p + r, c);
    }
}

// Bresenham, the end point is not drawn (GDI)
BOOL CDC::LineTo(int x1, int y1)
{
    uint32_t c = RmtPixel(m_pen ? m_pen->GetColor() : 0);
    if (m_recording && m_bitmap && m_bitmap->Bits()) {
        DrawOp op{};
        op.kind = DrawOp::LINE;
        op.l = m_curX, op.t = m_curY, op.r = x1, op.b = y1;
        op.color = c;
        m_ops.push_back(op);
    } else if (m_bitmap && m_bitmap->Bits()) {
        m_drawnOutside = true;
        LinePixels(m_curX, m_curY, x1, y1, c, nullptr);
    }
    m_curX = x1;
    m_curY = y1;
    return TRUE;
}

void CDC::LinePixels(int x, int y, int x1, int y1, uint32_t c, const std::vector<char>* dirtyTiles)
{
    const int w = m_bitmap->Width(), h = m_bitmap->Height();
    int dx = abs(x1 - x), dy = -abs(y1 - y);
    int sx = x < x1 ? 1 : -1, sy = y < y1 ? 1 : -1, err = dx + dy;
    while (x != x1 || y != y1) {
        if (x >= 0 && y >= 0 && x < w && y < h && (!dirtyTiles || (*dirtyTiles)[(y / TILE_H) * m_tilesX + x / TILE_W]))
            m_bitmap->Bits()[(size_t)y * w + x] = c;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

BOOL CDC::BitBlt(int x, int y, int w, int h, CDC* src, int xs, int ys, DWORD)
{
    return Blit(x, y, w, h, src, xs, ys, w, h);
}

// nearest neighbour
BOOL CDC::StretchBlt(int x, int y, int w, int h, CDC* src, int xs, int ys, int ws, int hs, DWORD)
{
    if ((w != ws || h != hs) && (w <= 0 || h <= 0)) return FALSE;
    return Blit(x, y, w, h, src, xs, ys, ws, hs);
}

BOOL CDC::Blit(int x, int y, int w, int h, CDC* src, int xs, int ys, int ws, int hs)
{
    const CBitmap* sb = src ? src->m_bitmap : nullptr;
    if (!m_bitmap || !m_bitmap->Bits() || !sb || !sb->Bits()) return FALSE;
    if (m_recording) {
        if (sb == m_bitmap) m_frameFull = true; // from its own pixels: only right when every call is drawn, in order
        DrawOp op{};
        op.kind = DrawOp::BLIT;
        op.l = x, op.t = y, op.r = x + w, op.b = y + h;
        op.src = sb;
        op.srcVersion = sb->Version();
        op.xs = xs, op.ys = ys, op.ws = ws, op.hs = hs;
        m_ops.push_back(op);
        return TRUE;
    }
    m_drawnOutside = true;
    BlitPixels(x, y, w, h, sb, xs, ys, ws, hs, BitmapRect());
    return TRUE;
}

void CDC::BlitPixels(int x, int y, int w, int h, const CBitmap* sb, int xs, int ys, int ws, int hs, const CRect& clip)
{
    if (w <= 0 || h <= 0) return;
    const int dw = m_bitmap->Width(), sw = sb->Width(), sh = sb->Height();
    int x0 = std::max(x, (int)clip.left), x1 = std::min(x + w, (int)clip.right);
    int y0 = std::max(y, (int)clip.top), y1 = std::min(y + h, (int)clip.bottom);
    if (w == ws && h == hs) {
        // also within the source, then whole rows
        x0 = std::max(x0, x - xs);
        x1 = std::min(x1, x - xs + sw);
        y0 = std::max(y0, y - ys);
        y1 = std::min(y1, y - ys + sh);
        if (x0 >= x1) return;
        for (int dy = y0; dy < y1; dy++)
            std::memmove(m_bitmap->Bits() + (size_t)dy * dw + x0, sb->Bits() + (size_t)(ys + dy - y) * sw + xs + x0 - x,
                         (size_t)(x1 - x0) * sizeof(uint32_t));
        return;
    }
    for (int dy = y0; dy < y1; dy++) {
        const int sy = ys + (int)((long long)(dy - y) * hs / h);
        if (sy < 0 || sy >= sh) continue;
        uint32_t* dp = m_bitmap->Bits() + (size_t)dy * dw;
        const uint32_t* sp = sb->Bits() + (size_t)sy * sw;
        for (int dx = x0; dx < x1; dx++) {
            const int sx = xs + (int)((long long)(dx - x) * ws / w);
            if (sx >= 0 && sx < sw) dp[dx] = sp[sx];
        }
    }
}

void CDC::DrawRecorded(const DrawOp& op, const CRect& clip)
{
    if (op.kind == DrawOp::FILL)
        FillPixels(op.l, op.t, op.r, op.b, op.color, clip);
    else
        BlitPixels(op.l, op.t, op.r - op.l, op.b - op.t, op.src, op.xs, op.ys, op.ws, op.hs, clip);
}

void CDC::BeginFrame()
{
    m_recording = true;
    m_frameFull = false;
    m_ops.clear();
}

// Each tile has the hash of the calls that draw into it, in their order: a tile with the hash of the previous
// frame would get the same pixels, and is left as it is
void CDC::EndFrame()
{
    m_recording = false;
    m_changed.clear();
    if (!m_bitmap || !m_bitmap->Bits()) {
        m_ops.clear();
        return;
    }
    const int w = m_bitmap->Width(), h = m_bitmap->Height();
    const int tilesX = (w + TILE_W - 1) / TILE_W, tilesY = (h + TILE_H - 1) / TILE_H;
    const bool full = m_frameFull || m_drawnOutside || tilesX != m_tilesX || tilesY != m_tilesY ||
                      m_frameBitmap != m_bitmap || m_frameBitmapVersion != m_bitmap->Version();
    m_tilesX = tilesX;
    m_tilesY = tilesY;
    m_frameBitmap = m_bitmap;
    m_frameBitmapVersion = m_bitmap->Version();
    m_drawnOutside = false;

    const CRect all = BitmapRect();
    auto clipped = [&all](CRect r) {
        r.left = std::max(r.left, all.left);
        r.top = std::max(r.top, all.top);
        r.right = std::min(r.right, all.right);
        r.bottom = std::min(r.bottom, all.bottom);
        return r;
    };
    const size_t tiles = (size_t)tilesX * tilesY;
    m_newTileHash.assign(tiles, 0);
    for (const DrawOp& op : m_ops) {
        const CRect b = clipped(op.Bounds());
        if (b.IsRectEmpty()) continue;
        const uint64_t hash = op.Hash();
        for (int ty = b.top / TILE_H; ty <= (b.bottom - 1) / TILE_H; ty++)
            for (int tx = b.left / TILE_W; tx <= (b.right - 1) / TILE_W; tx++) {
                uint64_t& t = m_newTileHash[(size_t)ty * tilesX + tx];
                t = HashMix(t, hash);
            }
    }
    m_dirty.assign(tiles, 0);
    bool any = false;
    for (size_t i = 0; i < tiles; i++) {
        m_dirty[i] = full || m_tileHash.size() != tiles || m_newTileHash[i] != m_tileHash[i];
        any |= m_dirty[i] != 0;
    }
    m_tileHash.swap(m_newTileHash);
    if (!any) {
        m_ops.clear();
        return;
    }

    // every call again, only into the dirty tiles (a run of them in a row of tiles at a time)
    for (const DrawOp& op : m_ops) {
        const CRect b = clipped(op.Bounds());
        if (b.IsRectEmpty()) continue;
        const int tx0 = b.left / TILE_W, tx1 = (b.right - 1) / TILE_W;
        const int ty0 = b.top / TILE_H, ty1 = (b.bottom - 1) / TILE_H;
        if (op.kind == DrawOp::LINE) {
            bool needed = false;
            for (int ty = ty0; ty <= ty1 && !needed; ty++)
                for (int tx = tx0; tx <= tx1 && !needed; tx++) needed = m_dirty[(size_t)ty * tilesX + tx];
            if (needed) LinePixels(op.l, op.t, op.r, op.b, op.color, &m_dirty);
            continue;
        }
        for (int ty = ty0; ty <= ty1; ty++) {
            const char* row = m_dirty.data() + (size_t)ty * tilesX;
            for (int tx = tx0; tx <= tx1; tx++) {
                if (!row[tx]) continue;
                int end = tx;
                while (end + 1 <= tx1 && row[end + 1]) end++;
                DrawRecorded(op, clipped(CRect(tx * TILE_W, ty * TILE_H, (end + 1) * TILE_W, (ty + 1) * TILE_H)));
                tx = end;
            }
        }
    }
    m_ops.clear();

    // the changed rectangles: the runs of dirty tiles of each row, joined with the same run of the row above
    for (int ty = 0; ty < tilesY; ty++) {
        const char* row = m_dirty.data() + (size_t)ty * tilesX;
        for (int tx = 0; tx < tilesX; tx++) {
            if (!row[tx]) continue;
            int end = tx;
            while (end + 1 < tilesX && row[end + 1]) end++;
            CRect r = clipped(CRect(tx * TILE_W, ty * TILE_H, (end + 1) * TILE_W, (ty + 1) * TILE_H));
            auto above = std::find_if(m_changed.begin(), m_changed.end(), [&r](const CRect& c) {
                return c.left == r.left && c.right == r.right && c.bottom == r.top;
            });
            if (above != m_changed.end())
                above->bottom = r.bottom;
            else
                m_changed.push_back(r);
            tx = end;
        }
    }
}

#include <thread>
#include <chrono>

void Sleep(DWORD ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

DWORD GetTickCount()
{
    return (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
