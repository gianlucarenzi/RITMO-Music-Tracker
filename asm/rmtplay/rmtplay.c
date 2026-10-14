/* rmtplay.c - the program of the XEX export with the RMT player of RITMO: the visualizer of RMTPLAY.COM of
 * VERA_ATARI_PBI (vera-tests/rmt/rmtplay.c) on the Atari screen (ANTIC), without the VERA parts.
 *
 *   NAME / AUTHOR / DATE of the song (the export dialog); a line longer than 40 characters scrolls left (coarse,
 *   one character at a time) after 5 seconds of play
 *   one volume bar per channel (4 for RMT4, 8 for RMT8: channels 5-8 play on the second POKEY): ANTIC mode 4
 *   rows on a black background, coloured row by row by a DLI
 *   AUDF / AUDC of every channel and AUDCTL, in hex
 *   song line, row in the track, speed, play time
 *
 *   SPACE  pause / play          R  restart the song          ESC  stop and exit
 *
 * Sound: the RMT player of the tracker driver (rmtxex.s, RMT 1.28 or Patch16) on the deferred VBI. The tracker
 * (SongExporter.cpp) puts the module after the program and fills rmt_song, rmt_song_end, rmt_song_ntsc and
 * rmt_info[]. RMT_TRACKS (4 or 8) is the build of the player (Makefile).
 */

#include <atari.h>
#include <conio.h>
#include <string.h>

#ifndef RMT_TRACKS
#define RMT_TRACKS 4
#endif

/* cc65 is much faster with static locals; nothing here is recursive. The main loop must fit in a frame next to the
 * player on the VBI (8 tracks at instrument speed 4: 4 calls of 20 lines): no multiplications, no long divisions,
 * only the cells that change are written. */
#pragma static-locals (on)

/* ---- the player (rmtxex.s), set by the tracker -------------------------- */

unsigned char __fastcall__ rmt_init(const void *module);
void rmt_vbi_on(void);
void rmt_vbi_off(void);
extern volatile unsigned int rmt_frames;

extern const unsigned char *rmt_song;               /* the module */
extern const unsigned char *rmt_song_end;           /* first byte after it */
extern volatile unsigned char *rmt_p_song;          /* next song line */
#pragma zpsym ("rmt_p_song")
extern volatile unsigned char rmt_abeat;            /* row in the track */
extern volatile unsigned char rmt_maxtracklen;      /* rows per track */
extern volatile unsigned char rmt_speed;            /* frames per row */
extern volatile unsigned char rmt_chan_audf[RMT_TRACKS];
extern volatile unsigned char rmt_chan_audc[RMT_TRACKS];
extern volatile unsigned char rmt_audctl;
#if RMT_TRACKS > 4
extern volatile unsigned char rmt_audctl2;
#endif

extern void rmt_dli(void);
extern unsigned char rmt_dli_pf0[];
extern unsigned char rmt_dli_count, rmt_dli_idx;

/* NAME, AUTHOR, DATE: written by the tracker, zero terminated */
#define INFO_MAX 200
char rmt_info[3][INFO_MAX + 1] = { "", "", "" };

/* ---- screen layout ---------------------------------------------------- */

#define ROW_TITLE   0
#define ROW_NAME    2
#define BAR_TOP     6       /* 8 rows of bars: 64 pixels, 4 per volume step */
#define BAR_ROWS    8
#define ROW_LABELS  (BAR_TOP + BAR_ROWS)
#define ROW_AUDF    16
#define ROW_AUDC    17
#define ROW_AUDCTL  18
#define ROW_POS     20
#define ROW_STATE   21
#define ROW_KEYS1   23

#if RMT_TRACKS > 4
#define BAR_W       3       /* 8 bars: 3 wide, 2 apart */
#define BAR_STEP    5
#define BAR_X0      1
#else
#define BAR_W       6       /* 4 bars: 6 wide, 3 apart */
#define BAR_STEP    9
#define BAR_X0      3
#endif

/* bar glyphs: screen codes $41-$48 (ATASCII ctrl-A..ctrl-H, unused here) */
#define GLYPH_BASE  0x40

static const char hexdig[] = "0123456789ABCDEF";
/* the hex digits as screen codes */
static const unsigned char hexcode[16] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26
};

static unsigned char *scr;
static unsigned char *row_ptr[24];          /* the screen rows */
#pragma bss-name (push, "LOWBSS")
static unsigned char fontbuf[2048];         /* 1 KB font, aligned inside (low memory, rmtplay.cfg) */
#pragma bss-name (pop)
static unsigned char *font;                 /* ANTIC font (CHBAS) */
static unsigned char bar_px[RMT_TRACKS];    /* height shown, with fall-off */
static unsigned char bar_shown[RMT_TRACKS]; /* height drawn on screen */
static unsigned char line_buf[40];

/* value shown in each field: a field is written again only when it changes ($FFFF = not drawn yet) */
enum {
    F_AUDF = 0, F_AUDC = 8, F_CTL = 16, F_LINE = 18, F_LINES, F_ROW, F_ROWS,
    F_SPEED, F_SEC, F_PAUSE, F_COUNT
};
static unsigned int shown[F_COUNT];

static unsigned char changed(unsigned char f, unsigned int v)
{
    if (shown[f] == v)
        return 0;
    shown[f] = v;
    return 1;
}
static unsigned char song_shift;            /* bytes per song line: 1 << shift */
static const unsigned char *song_start;
static unsigned int song_lines;

/* ---- screen helpers --------------------------------------------------- */

static unsigned char screen_code(unsigned char c)
{
    if (c < 32)
        return c + 64;
    if (c < 96)
        return c - 32;
    return c;
}

static void put_codes(unsigned char x, unsigned char y, const unsigned char *c, unsigned char n)
{
    memcpy(row_ptr[y] + x, c, n);
}

static void put_text(unsigned char x, unsigned char y, const char *s, unsigned char inv)
{
    unsigned char n = 0;

    while (s[n] && x + n < 40) {
        line_buf[n] = screen_code((unsigned char)s[n]) | inv;
        n++;
    }
    put_codes(x, y, line_buf, n);
}

static void put_center(unsigned char y, const char *s)
{
    unsigned char n = (unsigned char)strlen(s);

    put_text(n < 40 ? (40 - n) / 2 : 0, y, s, 0);
}

static void put_hex(unsigned char x, unsigned char y, unsigned char v)
{
    line_buf[0] = screen_code(hexdig[v >> 4]);
    line_buf[1] = screen_code(hexdig[v & 15]);
    put_codes(x, y, line_buf, 2);
}

/* two decimal digits of v (0..99): the tens by subtraction (the division of cc65 takes hundreds of cycles) */
static void put_dec2(unsigned char x, unsigned char y, unsigned char v)
{
    unsigned char *p = row_ptr[y] + x;
    unsigned char t = 0x10;     /* screen code of '0' */

    while (v >= 10) {
        v -= 10;
        ++t;
    }
    p[0] = t;
    p[1] = 0x10 + v;
}

/* ---- font with the bar glyphs ----------------------------------------- */

static void make_font(void)
{
    unsigned char h, r, b;

    font = (unsigned char *)(((unsigned int)fontbuf + 1023) & 0xFC00);
    memcpy(font, (void *)0xE000, 1024);
    /* glyph h (1..8), drawn in ANTIC mode 4 (4 pixels of 2 bits per byte): the bottom h pixel rows lit, as LED
     * segments of 3 lit rows and 1 dark row (one segment per volume step). Lit row = 3 pixels of colour 01
     * (COLPF0) and 1 of background, so the bar columns stay apart. */
    for (h = 1; h <= 8; h++)
        for (r = 0; r < 8; r++) {
            b = 7 - r;      /* pixel row counted from the bottom */
            font[(GLYPH_BASE + h) * 8 + r] = (b < h && (b & 3) != 3) ? 0x54 : 0x00;
        }
    OS.chbas = (unsigned int)font >> 8;
}

/* ---- DLI colours on the bar rows --------------------------------------- */

static unsigned char *dl_line(unsigned char row)
{
    unsigned char *dl = (unsigned char *)OS.sdlst;

    return dl + (row ? 5 + row : 3);    /* $70 $70 $70 $42 lo hi $02... */
}

/* COLPF0 of each bar row: the bars are ANTIC mode 4 rows, whose background is COLBK (black), so only the bars
 * change colour */
static const unsigned char bar_color[BAR_ROWS] = {
    0x34, 0x36, 0x28, 0x1A, 0xEA, 0xDA, 0xC8, 0xC6     /* top red -> bottom green */
};

static void dli_on(void)
{
    unsigned char i;

    for (i = 0; i < BAR_ROWS; i++)
        rmt_dli_pf0[i] = bar_color[i];
    rmt_dli_count = BAR_ROWS;
    rmt_dli_idx = 0;

    /* bar rows in ANTIC mode 4 (multicolour text, same 40 bytes per row); DLI on the line before each bar row */
    for (i = BAR_TOP; i < BAR_TOP + BAR_ROWS; i++)
        *dl_line(i) = 0x04;
    for (i = BAR_TOP - 1; i < BAR_TOP + BAR_ROWS - 1; i++)
        *dl_line(i) |= 0x80;

    /* start below the bars, so the first DLI is the first of a frame */
    while (ANTIC.vcount < 100)
        ;
    OS.vdslst = rmt_dli;
    ANTIC.nmien = NMIEN_DLI | NMIEN_VBI;
}

static void dli_off(void)
{
    unsigned char i;

    ANTIC.nmien = NMIEN_VBI;
    *dl_line(BAR_TOP - 1) &= 0x7F;
    for (i = BAR_TOP; i < BAR_TOP + BAR_ROWS; i++)
        *dl_line(i) = 0x02;     /* back to the OS text mode */
}

/* ---- static parts of the screen ---------------------------------------- */

#define SCROLL_WAIT     5       /* seconds of play before a long line scrolls */
#define SCROLL_SHIFT    3       /* one character every 8 frames */
#define SCROLL_GAP      40      /* blanks between the end and the start again: a whole row, so the text leaves
                                 * the screen before it comes back in from the right */

/* NAME, AUTHOR, DATE: on rows ROW_NAME, ROW_NAME + 1, ROW_NAME + 2 */
static unsigned char info_len[3];
static unsigned char info_shown[3];         /* scroll offset drawn */
#if SCROLL_GAP < 40
#error "info_render() needs the blanks of a whole row"
#endif

/* The 40 cells of a long info line from scroll offset k, written to the screen. rmt_info[i] holds its screen codes
 * (draw_static), the SCROLL_GAP blanks follow: with a gap of a whole row the 40 cells are the text then blanks, or
 * blanks then the text again, a copy and a fill. Building the row a character at a time (and then comparing it with
 * the one on the screen) took about 200 scan lines at every step of a scroll. */
static void info_render(unsigned char i, unsigned char k)
{
    unsigned char *dst = row_ptr[ROW_NAME + i];
    const unsigned char *s = (const unsigned char *)rmt_info[i];
    unsigned char len = info_len[i], n;

    if (k < len) {
        n = len - k;
        if (n > 40)
            n = 40;
        memcpy(dst, s + k, n);
        memset(dst + n, 0, 40 - n);
    } else {
        n = len + SCROLL_GAP - k;   /* blanks before the text comes round again: 1..40 */
        memset(dst, 0, n);
        memcpy(dst + n, s, 40 - n);
    }
}

static void draw_static(void)
{
    unsigned char i, x;
    char title[41];

    memset(title, ' ', 40);
    title[40] = 0;
    memcpy(title + 1, "RITMO PLAYER", 12);
#if RMT_TRACKS > 4
    memcpy(title + 21, "RMT8 2 POKEY", 12);
#else
    memcpy(title + 31, "RMT4", 4);
#endif
    memcpy(title + 36, OS.palnts ? "PAL " : "NTSC", 4);
    put_text(0, ROW_TITLE, title, 0x80);

    /* a line longer than the screen shows its first 40 characters until draw_info() starts scrolling it */
    for (i = 0; i < 3; i++) {
        info_len[i] = (unsigned char)strlen(rmt_info[i]);
        if (info_len[i] <= 40) {
            put_center(ROW_NAME + i, rmt_info[i]);
            continue;
        }
        for (x = 0; x < info_len[i]; x++)      /* the long lines as screen codes, once */
            rmt_info[i][x] = screen_code((unsigned char)rmt_info[i][x]);
        info_shown[i] = 0;
        info_render(i, 0);
    }

    for (i = 0; i < RMT_TRACKS; i++) {
        x = BAR_X0 + i * BAR_STEP + (BAR_W - 2) / 2;
#if RMT_TRACKS > 4
        line_buf[0] = screen_code(i < 4 ? 'L' : 'R');
        line_buf[1] = screen_code('1' + (i & 3));
        put_codes(x, ROW_LABELS, line_buf, 2);
#else
        line_buf[0] = screen_code('1' + i);
        put_codes(x + 1, ROW_LABELS, line_buf, 1);
#endif
    }

    put_text(0, ROW_AUDF, "AUDF", 0);
    put_text(0, ROW_AUDC, "AUDC", 0);
    put_text(0, ROW_AUDCTL, "AUDCTL", 0);
    put_text(0, ROW_POS, "LINE   /      ROW   /     SPEED", 0);
    put_text(0, ROW_STATE, "TIME   :", 0);
    put_text(0, ROW_KEYS1, "SPACE PAUSE    R RESTART    ESC EXIT", 0);
}

/* ---- dynamic parts ----------------------------------------------------- */

/* Coarse scroll of the info lines longer than 40 characters: the text and SCROLL_GAP blanks go round in the 40
 * columns of the row, after SCROLL_WAIT seconds of play one character every 2^SCROLL_SHIFT frames. It follows the
 * frames played (it stops in pause and starts again from 0 when the song is restarted), kept step by step: the
 * offset from the total of the frames took a 32 bit shift and a 16 bit modulo per line every other frame. Only the
 * row is written at each step only (info_render). */
static unsigned int info_wait;              /* frames of play before the long lines scroll */
static unsigned char info_sub;              /* frames since the last scroll step */

static void info_reset(unsigned char fps)
{
    unsigned char i;

    info_wait = fps * SCROLL_WAIT;
    info_sub = 0;
    for (i = 0; i < 3; i++)
        if (info_len[i] > 40 && info_shown[i]) {
            info_shown[i] = 0;
            info_render(i, 0);
        }
}

static void draw_info(unsigned char adv)    /* frames played since the last call */
{
    unsigned char i, k, n, period;

    if (info_wait) {
        if (info_wait > adv) {
            info_wait -= adv;
            return;
        }
        adv -= (unsigned char)info_wait;
        info_wait = 0;
    }
    info_sub += adv;
    n = info_sub >> SCROLL_SHIFT;
    if (!n)
        return;
    info_sub &= (1 << SCROLL_SHIFT) - 1;
    for (i = 0; i < 3; i++) {
        if (info_len[i] <= 40)
            continue;
        period = info_len[i] + SCROLL_GAP;
        k = info_shown[i] + n;
        while (k >= period)
            k -= period;
        info_shown[i] = k;
        info_render(i, k);
    }
}

/* the volume of each channel as it is now; a bar falls 2 pixels per frame (elapsed: frames since the last call) */
/* the left column of each bar */
static const unsigned char bar_x[RMT_TRACKS] = {
#if RMT_TRACKS > 4
    BAR_X0, BAR_X0 + BAR_STEP, BAR_X0 + 2 * BAR_STEP, BAR_X0 + 3 * BAR_STEP,
    BAR_X0 + 4 * BAR_STEP, BAR_X0 + 5 * BAR_STEP, BAR_X0 + 6 * BAR_STEP, BAR_X0 + 7 * BAR_STEP
#else
    BAR_X0, BAR_X0 + BAR_STEP, BAR_X0 + 2 * BAR_STEP, BAR_X0 + 3 * BAR_STEP
#endif
};

/* Only the rows between the old and the new top of a bar are written: going through the 8 rows of every bar that
 * moved took about 120 scan lines a frame with 8 tracks at instrument speed 4 (most bars move every frame), and with
 * the player on the VBI the main loop got through a frame only one time in two. */
static void draw_bars(unsigned char paused, unsigned char elapsed)
{
    unsigned char i, r, last, h, g, target, fall;
    unsigned char *p;

    fall = elapsed > 32 ? 64 : elapsed << 1;
    for (i = 0; i < RMT_TRACKS; i++) {
        target = paused ? 0 : (unsigned char)((rmt_chan_audc[i] & 0x0F) << 2);
        h = bar_px[i];
        if (target >= h)
            h = target;
        else
            h = (h - target > fall) ? h - fall : target;
        bar_px[i] = h;

        g = bar_shown[i];
        if (h == g)
            continue;       /* only the bars that moved */
        bar_shown[i] = h;
        if (g < h) {        /* r = 0 is the bottom row */
            r = g >> 3;
            last = (h - 1) >> 3;
        } else {
            r = h >> 3;
            last = (g - 1) >> 3;
        }
        p = row_ptr[BAR_TOP + BAR_ROWS - 1 - r] + bar_x[i];
        for (; r <= last; r++) {
            g = r << 3;     /* pixels of the bar in row r */
            g = h > g ? h - g : 0;
            if (g > 8)
                g = 8;
            if (g)
                g += GLYPH_BASE;
            p[0] = g;
            p[1] = g;
            p[2] = g;
#if BAR_W > 3
            p[3] = g;
            p[4] = g;
            p[5] = g;
#endif
            p -= 40;
        }
    }
}

/* the registers shown, written straight to the screen (the hot path of the main loop) */
static unsigned char reg_shown[2 * RMT_TRACKS + 2];

static void draw_regs(void)
{
    unsigned char i, v;
    unsigned char *f = row_ptr[ROW_AUDF] + 5, *c = row_ptr[ROW_AUDC] + 5;

    for (i = 0; i < RMT_TRACKS; i++) {
#if RMT_TRACKS > 4
        if (i == 4) {
            f += 2;     /* gap between the first and the second POKEY */
            c += 2;
        }
#endif
        v = rmt_chan_audf[i];
        if (v != reg_shown[i]) {
            reg_shown[i] = v;
            f[0] = hexcode[v >> 4];
            f[1] = hexcode[v & 15];
        }
        v = rmt_chan_audc[i];
        if (v != reg_shown[RMT_TRACKS + i]) {
            reg_shown[RMT_TRACKS + i] = v;
            c[0] = hexcode[v >> 4];
            c[1] = hexcode[v & 15];
        }
        f += 3;
        c += 3;
    }
    if (changed(F_CTL, rmt_audctl))
        put_hex(7, ROW_AUDCTL, rmt_audctl);
#if RMT_TRACKS > 4
    if (changed(F_CTL + 1, rmt_audctl2))
        put_hex(10, ROW_AUDCTL, rmt_audctl2);
#endif
}

static void draw_position(void)
{
    const unsigned char *a, *b;
    unsigned int line;

    /* the VBI may move the pointer between the two byte reads */
    do {
        a = (const unsigned char *)rmt_p_song;
        b = (const unsigned char *)rmt_p_song;
    } while (a != b);
    line = (unsigned int)(a - song_start) >> song_shift;
    line = line ? line - 1 : 0;     /* p_song already points to the next line */

    if (changed(F_LINE, (unsigned char)line))
        put_hex(5, ROW_POS, (unsigned char)line);
    if (changed(F_LINES, (unsigned char)(song_lines - 1)))
        put_hex(8, ROW_POS, (unsigned char)(song_lines - 1));
    if (changed(F_ROW, rmt_abeat))
        put_hex(18, ROW_POS, rmt_abeat);
    if (changed(F_ROWS, (unsigned char)(rmt_maxtracklen - 1)))
        put_hex(21, ROW_POS, (unsigned char)(rmt_maxtracklen - 1));
    if (changed(F_SPEED, rmt_speed))
        put_hex(32, ROW_POS, rmt_speed);
}

/* the play time: seconds counted from the frames, minutes and seconds kept apart (no divisions) */
static unsigned char time_frames, time_sec, time_min;

static void count_time(unsigned char elapsed, unsigned char fps)
{
    time_frames += elapsed;
    while (time_frames >= fps) {
        time_frames -= fps;
        if (++time_sec == 60) {
            time_sec = 0;
            if (++time_min == 100)
                time_min = 0;
        }
    }
}

static void draw_state(unsigned char paused)
{
    if (changed(F_SEC, ((unsigned int)time_min << 8) | time_sec)) {
        put_dec2(5, ROW_STATE, time_min);
        put_dec2(8, ROW_STATE, time_sec);
    }
    if (changed(F_PAUSE, paused))
        put_text(33, ROW_STATE, paused ? "PAUSED" : "      ", 0x80 * paused);
}

/* ---- main --------------------------------------------------------------- */

int main(void)
{
    unsigned char paused = 0;
    unsigned char fps = OS.palnts ? 50 : 60;
    unsigned char tick, last, k, i, elapsed, odd = 0;
    unsigned char old_color1 = OS.color1, old_color2 = OS.color2, old_color4 = OS.color4;
    unsigned char old_chbas = OS.chbas;
    unsigned char adv = 0;          /* frames played since the last draw_info() */

#if RMT_TRACKS > 4
    song_shift = 3;     /* 8 bytes per song line */
#else
    song_shift = 2;     /* 4 bytes per song line */
#endif
    song_start = (const unsigned char *)(rmt_song[14] | (rmt_song[15] << 8));
    song_lines = (unsigned int)(rmt_song_end - song_start) >> song_shift;

    OS.crsinh = 1;
    clrscr();
    scr = OS.savmsc;
    for (i = 0; i < 24; i++)
        row_ptr[i] = scr + i * 40;
    memset(reg_shown, 0xFF, sizeof reg_shown);
    OS.color1 = 0x0C;
    OS.color2 = 0x00;
    OS.color4 = 0x00;
    memset(shown, 0xFF, sizeof shown);
    make_font();
    draw_static();
    info_reset(fps);

    rmt_init(rmt_song);
    rmt_vbi_on();
    dli_on();

    last = OS.rtclok[2];
    for (;;) {
        /* once per frame */
        while ((tick = OS.rtclok[2]) == last)
            ;
        elapsed = (unsigned char)(tick - last);
        if (!paused) {
            adv += elapsed;
            count_time(elapsed, fps);
        }
        last = tick;

        /* the bars every frame, the registers and the text fields every other frame */
        draw_bars(paused, elapsed);
        odd ^= 1;
        if (odd) {
            draw_regs();
        } else {
            draw_position();
            draw_state(paused);
            draw_info(adv);
            adv = 0;
        }

        if (!kbhit())
            continue;
        k = cgetc();
        if (k == 27)
            break;
        if (k == ' ') {
            paused = !paused;
            if (paused)
                rmt_vbi_off();      /* silences POKEY, keeps the position */
            else
                rmt_vbi_on();
        }
        else if (k == 'r' || k == 'R') {
            rmt_vbi_off();
            rmt_init(rmt_song);
            adv = 0;
            info_reset(fps);
            time_frames = time_sec = time_min = 0;
            paused = 0;
            rmt_vbi_on();
        }
    }

    rmt_vbi_off();
    dli_off();
    OS.chbas = old_chbas;
    OS.color1 = old_color1;
    OS.color2 = old_color2;
    OS.color4 = old_color4;
    OS.crsinh = 0;
    clrscr();
    cputs("Music stopped.\r\n");
    return 0;
}
