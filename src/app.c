#include "app.h"
#include "platform.h"
#include "browser.h"
#include "renderer.h"
#include "framebuffer.h"
#include "network.h"
#include "html.h"
#include "font.h"
#include "logo_data.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <direct.h>

#include <windows.h>
#include <xinput.h>

#ifdef PLATFORM_XBOX360
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define XINPUT_GAMEPAD_A              0x1000
#define XINPUT_GAMEPAD_B              0x2000
#define XINPUT_GAMEPAD_X              0x4000
#define XINPUT_GAMEPAD_Y              0x8000
#define XINPUT_GAMEPAD_LEFT_SHOULDER  0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_START          0x0010
#define XINPUT_GAMEPAD_BACK           0x0020

#define AERO_TAB_HEIGHT      32
#define AERO_TOOLBAR_HEIGHT  40
#define AERO_BOOKMARK_HEIGHT 28
#define AERO_STATUS_HEIGHT   24
#define AERO_BTN_W           32
#define AERO_BTN_H           28

static const uint32_t AERO_TOOLBAR_BG  = 0xFFF0F2F5;
static const uint32_t AERO_TAB_BG      = 0xFFE8EAED;
static const uint32_t AERO_ACTIVE_TAB  = 0xFFFFFFFF;
static const uint32_t AERO_BORDER      = 0xFFB0B8C0;
static const uint32_t AERO_URL_BG      = 0xFFFFFFFF;
static const uint32_t AERO_BTN_HOVER   = 0xFFD0D4D8;
static const uint32_t AERO_BTN_PRESS   = 0xFFC0C4C8;

static const uint32_t AERO_TEXT       = 0xFF101010;
static const uint32_t AERO_LINK       = 0xFF1155CC;
static const uint32_t AERO_CONTENT_BG = 0xFFFFFFFF;

#define COOKIE_JAR_SIZE 64
#define CACHE_MAX_ENTRIES 32
#define CACHE_PATH_MAX 512

typedef struct {
    char domain[128];
    char name[128];
    char value[512];
    char path[256];
    bool http_only;
    bool secure;
} CookieEntry;

typedef struct {
    char url[2048];
    char path[CACHE_PATH_MAX];
    int64_t timestamp;
    int64_t size;
} CacheEntry;

static CookieEntry g_cookie_jar[COOKIE_JAR_SIZE];
static int g_cookie_count = 0;
static CacheEntry g_cache[CACHE_MAX_ENTRIES];
static int g_cache_count = 0;

static bool g_deferred_active = false;
#define MAX_DEFERRED_TEXT 2048
typedef struct { int x, y; char text[256]; int font_size; uint32_t color; bool bold; bool clipped; int max_w; } DeferredText;
static DeferredText g_deferred[MAX_DEFERRED_TEXT];
static int g_deferred_count = 0;

static void deferred_text_draw(int x, int y, const char* text, int font_size, uint32_t color, bool bold) {
    if (!g_deferred_active || !text || g_deferred_count >= MAX_DEFERRED_TEXT) return;
    DeferredText* d = &g_deferred[g_deferred_count++];
    d->x = x; d->y = y; d->font_size = font_size; d->color = color; d->bold = bold;
    d->clipped = false; d->max_w = 0;
    strncpy(d->text, text, 255); d->text[255] = '\0';
}

static void deferred_text_draw_clipped(int x, int y, int max_w, const char* text, int font_size, uint32_t color, bool bold) {
    if (!g_deferred_active || !text || g_deferred_count >= MAX_DEFERRED_TEXT) return;
    DeferredText* d = &g_deferred[g_deferred_count++];
    d->x = x; d->y = y; d->font_size = font_size; d->color = color; d->bold = bold;
    d->clipped = true; d->max_w = max_w;
    strncpy(d->text, text, 255); d->text[255] = '\0';
}

// Run a modal XUI chrome session and apply the selected action to the browser.
// Action ids mirror xui_ui.h XuiChromeAction (XUI_CHROME_* modes are 1..3).
#define XCH_BACK 1
#define XCH_FORWARD 2
#define XCH_REFRESH 3
#define XCH_HOME 4
#define XCH_NEW_TAB 5
#define XCH_SETTINGS 6
#define XCH_HOMEPAGE_DEFAULT 13
#define XCH_HOMEPAGE_NETXBOX 14
#define XCH_HOMEPAGE_DUCKDUCKGO 15
#define XCH_TOGGLE_TOOLBAR 16
static void chrome_session(AppContext* ctx, int mode) {
    ctx->chrome_ok = false;
    if (!ctx->api->run_chrome) return;
    int action = ctx->api->run_chrome(mode);
    if (action < 0) return;   // XUI chrome unavailable / failed to load
    ctx->chrome_ok = true;
    BrowserTab* tab = browser_get_active_tab(&ctx->browser);
    switch (action) {
        case XCH_BACK:      if (tab) browser_go_back(&ctx->browser); break;
        case XCH_FORWARD:   if (tab) browser_go_forward(&ctx->browser); break;
        case XCH_REFRESH:   if (tab) browser_refresh(&ctx->browser); break;
        case XCH_HOME:      if (tab) browser_go_home(&ctx->browser); break;
        case XCH_NEW_TAB:   browser_create_tab(&ctx->browser, BROWSER_HOME_URL); break;
        case XCH_SETTINGS:  ctx->show_settings = !ctx->show_settings; break;
        case XCH_HOMEPAGE_DEFAULT:  ctx->settings_homepage = 0; break;
        case XCH_HOMEPAGE_NETXBOX:  ctx->settings_homepage = 1; break;
        case XCH_HOMEPAGE_DUCKDUCKGO: ctx->settings_homepage = 2; break;
        case XCH_TOGGLE_TOOLBAR: ctx->settings_show_toolbar = !ctx->settings_show_toolbar; break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Software on-screen keyboard (canvas-drawn; no XUI dependency). Opens from the
// URL bar, search box, or START/select. Cursor + A clicks a key; B cancels.
// ---------------------------------------------------------------------------
static bool is_cursor_over(AppContext* ctx, int x, int y, int w, int h);
static bool is_ad_blocked(AppContext* ctx, const char* url);
#define OSKB_ROWS 5
#define OSKB_COLS 14
#define OSKB_KW  58
#define OSKB_KH  44
#define OSKB_KG   6
#define OSKB_PAD  14

static const char* g_oskb_rows[OSKB_ROWS] = {
    "1234567890",       // row 0: [+]) with Backspace handled specially below
    "qwertyuiop",       // row 1
    "asdfghjkl",        // row 2: + Enter
    "zxcvbnm",          // row 3: + Shift, + .com
    ". , / ? ! @ # $ % & * () " // row 4 (shifted used for row 3 upper? no)
};

// Define key kind by row/col, since special keys sit at fixed positions.
typedef enum { OSKB_CHAR, OSKB_BACKSPACE, OSKB_ENTER, OSKB_SHIFT, OSKB_SPACE, OSKB_COM } OsKbKind;
typedef struct {
    bool active;
    char ch;       // lowercase char, or 0 for special
    OsKbKind kind;
} OsKbKey;

static void os_kb_getkey(int row, int col, OsKbKey* out)
{
    out->active = false;
    out->ch = 0;
    out->kind = OSKB_CHAR;

    const char* r = (row < OSKB_ROWS) ? g_oskb_rows[row] : "";
    // Column offsets (indent) and per-row handling.
    switch (row)
    {
        case 0:
        {
            // 10 digits + "Backspace" in the remaining 4 columns.
            if (col < 10) { out->active = true; out->ch = r[col]; }
            else if (col == 12) { out->active = true; out->kind = OSKB_BACKSPACE; }
            break;
        }
        case 1:
            if (col < 10) { out->active = true; out->ch = r[col]; }
            break;
        case 2:
            if (col < 9) { out->active = true; out->ch = r[col]; }
            else if (col == 12) { out->active = true; out->kind = OSKB_ENTER; }
            break;
        case 3:
            if (col == 0) { out->active = true; out->kind = OSKB_SHIFT; }
            else if (col >= 1 && col < 8) { out->active = true; out->ch = r[col - 1]; }
            else if (col == 12) { out->active = true; out->kind = OSKB_COM; }
            break;
        case 4:
            if (col == 0) { out->active = true; out->kind = OSKB_SHIFT; }
            else if (col >= 1 && col <= 12) { out->active = true; out->ch = " /?.,:;!@#$%&*()"[col - 1]; }
            else if (col == 13) { out->active = true; out->kind = OSKB_BACKSPACE; }
            break;
    }
}

static void os_kb_rect(AppContext* ctx, int row, int col, int* x, int* y, int* w, int* h)
{
    int kw = OSKB_KW, kh = OSKB_KH, gap = OSKB_KG, pad = OSKB_PAD;
    int cols = OSKB_COLS;
    int total_w = cols * kw + (cols - 1) * gap + pad * 2;
    int total_h = OSKB_ROWS * kh + (OSKB_ROWS - 1) * gap + pad * 2 + 40 /*title*/;
    int panel_x = ctx->win_w / 2 - total_w / 2;
    int panel_y = ctx->win_h - total_h - 12;

    OsKbKey key;
    os_kb_getkey(row, col, &key);
    int left_extra = 0, width_extra = 0;
    if (key.kind == OSKB_BACKSPACE)      { left_extra = -kw - 2 * gap; width_extra = kw + 2 * gap; }
    else if (key.kind == OSKB_ENTER)     { left_extra = -2 * (kw + gap); width_extra = 2 * (kw + gap); }
    else if (key.kind == OSKB_SHIFT)     { /* first col: keep */ }
    else if (key.kind == OSKB_COM)       { left_extra = -3 * (kw + gap); width_extra = 3 * (kw + gap); }

    int base_x = panel_x + pad + col * (kw + gap);
    *x = base_x + left_extra;
    *y = panel_y + 40 + pad + row * (kh + gap);
    *w = kw + width_extra;
    *h = kh;
}

static void os_kb_open(AppContext* ctx, int target, const char* def)
{
    ctx->os_kb_active = true;
    ctx->os_kb_target = target;
    ctx->os_kb_shift = false;
    ctx->os_kb_cursor = 0;
    ctx->os_kb_text[0] = '\0';
    if (def)
    {
        strncpy(ctx->os_kb_text, def, sizeof(ctx->os_kb_text) - 1);
        ctx->os_kb_text[sizeof(ctx->os_kb_text) - 1] = '\0';
        ctx->os_kb_cursor = (int)strlen(ctx->os_kb_text);
    }
}

static void os_kb_apply(AppContext* ctx)
{
    char* text = ctx->os_kb_text;
    ctx->os_kb_active = false;
    if (ctx->os_kb_target == 0)
    {
        // URL
        if (text[0])
        {
            const char* resolved = browser_resolve_url(&ctx->browser, text);
            BrowserTab* tab = browser_get_active_tab(&ctx->browser);
            if (tab && resolved && !is_ad_blocked(ctx, resolved))
                browser_navigate(tab, resolved);
        }
        ctx->url_bar_focused = false;
        ctx->url_input[0] = '\0';
    }
    else
    {
        // Search
        if (text[0])
        {
            char u[2200];
            _snprintf(u, sizeof(u), "https://www.mojeek.com/search?q=%s", text);
            BrowserTab* tab = browser_get_active_tab(&ctx->browser);
            if (tab && !is_ad_blocked(ctx, u))
                browser_navigate(tab, u);
        }
        ctx->search_focused = false;
        ctx->search_input[0] = '\0';
    }
}

static void os_kb_cancel(AppContext* ctx)
{
    ctx->os_kb_active = false;
}

static void os_kb_type(AppContext* ctx, char c)
{
    int len = (int)strlen(ctx->os_kb_text);
    if (len < (int)sizeof(ctx->os_kb_text) - 2)
    {
        memmove(&ctx->os_kb_text[ctx->os_kb_cursor + 1], &ctx->os_kb_text[ctx->os_kb_cursor], len - ctx->os_kb_cursor + 1);
        ctx->os_kb_text[ctx->os_kb_cursor++] = c;
    }
}

static void os_kb_backspace(AppContext* ctx)
{
    int len = (int)strlen(ctx->os_kb_text);
    if (len > 0 && ctx->os_kb_cursor > 0)
    {
        memmove(&ctx->os_kb_text[ctx->os_kb_cursor - 1], &ctx->os_kb_text[ctx->os_kb_cursor], len - ctx->os_kb_cursor + 1);
        ctx->os_kb_cursor--;
    }
}

static void os_kb_handle_click(AppContext* ctx)
{
    for (int row = 0; row < OSKB_ROWS; row++)
    {
        for (int col = 0; col < OSKB_COLS; col++)
        {
            OsKbKey key;
            os_kb_getkey(row, col, &key);
            if (!key.active) continue;
            int x, y, w, h;
            os_kb_rect(ctx, row, col, &x, &y, &w, &h);
            if (is_cursor_over(ctx, x, y, w, h))
            {
                switch (key.kind)
                {
                    case OSKB_CHAR:
                        os_kb_type(ctx, ctx->os_kb_shift ? (char)toupper(key.ch) : key.ch);
                        ctx->os_kb_shift = false;
                        break;
                    case OSKB_BACKSPACE: os_kb_backspace(ctx); break;
                    case OSKB_SHIFT:     ctx->os_kb_shift = !ctx->os_kb_shift; break;
                    case OSKB_COM:       os_kb_type(ctx, '.'); os_kb_type(ctx, 'c'); os_kb_type(ctx, 'o'); os_kb_type(ctx, 'm'); break;
                    case OSKB_ENTER:     os_kb_apply(ctx); break;
                    case OSKB_SPACE:     os_kb_type(ctx, ' '); break;
                }
                return;
            }
        }
    }
}

static void os_kb_render(AppContext* ctx)
{
    Framebuffer* fb = &ctx->fb;
    int kw = OSKB_KW, kh = OSKB_KH, gap = OSKB_KG, pad = OSKB_PAD;
    int cols = OSKB_COLS, rows = OSKB_ROWS;
    int total_w = cols * kw + (cols - 1) * gap + pad * 2;
    int total_h = rows * kh + (rows - 1) * gap + pad * 2 + 40;
    int panel_x = ctx->win_w / 2 - total_w / 2;
    int panel_y = ctx->win_h - total_h - 12;

    fb_fill_rect_alpha(fb, panel_x, panel_y, total_w, total_h, 0xDDFFFFFF);
    fb_draw_rect(fb, panel_x, panel_y, total_w, total_h, 0xFF909090);

    // Title + text preview
    g_deferred_active = true;
    deferred_text_draw(panel_x + pad, panel_y + 10, ctx->os_kb_target == 0 ? "Enter URL" : "Search", 16, 0xFF333333, true);
    char cur[2050];
    strncpy(cur, ctx->os_kb_text, sizeof(cur) - 1);
    cur[sizeof(cur) - 1] = '\0';
    int pre = ctx->os_kb_cursor;
    if (pre < 0) pre = 0;
    if (pre > (int)strlen(cur)) pre = (int)strlen(cur);
    cur[pre] = '\0';
    deferred_text_draw(panel_x + pad, panel_y + 32, pre ? cur : " ", 20, 0xFF000000, false);
    // caret marker following the typed text
    int caretx = panel_x + pad + (pre ? ctx->api->text_measure(cur, 20, false) : 0);
    deferred_text_draw(caretx, panel_y + 32, "|", 20, 0xFF3399FF, true);
    g_deferred_active = false;

    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            OsKbKey key;
            os_kb_getkey(row, col, &key);
            if (!key.active) continue;
            int x, y, w, h;
            os_kb_rect(ctx, row, col, &x, &y, &w, &h);
            bool hover = is_cursor_over(ctx, x, y, w, h);
            uint32_t bg = hover ? 0xFFD8DDE3 : 0xFFEDEFF2;
            if (key.kind == OSKB_SHIFT && ctx->os_kb_shift) bg = 0xFF3399FF;
            fb_fill_rect(fb, x, y, w, h, bg);
            fb_draw_rect(fb, x, y, w, h, 0xFF9AA3AC);
            char label[8];
            switch (key.kind)
            {
                case OSKB_CHAR:
                    label[0] = ctx->os_kb_shift ? (char)toupper(key.ch) : key.ch;
                    label[1] = '\0';
                    break;
                case OSKB_BACKSPACE: strcpy(label, "Del"); break;
                case OSKB_ENTER:     strcpy(label, "OK"); break;
                case OSKB_SHIFT:     strcpy(label, "Shift"); break;
                case OSKB_COM:       strcpy(label, ".com"); break;
                case OSKB_SPACE:     strcpy(label, "Space"); break;
                default:             strcpy(label, ""); break;
            }
            int tx = x + (w - ctx->api->text_measure(label, 14, false)) / 2;
            int ty = y + h / 2 - 7;
            g_deferred_active = true;
            deferred_text_draw(tx, ty, label, 14, 0xFF222222, false);
            g_deferred_active = false;
        }
    }
}

static void flush_deferred_text(AppContext* ctx) {
    const Font* font = font_get_default();
    if (!font) return;
    for (int i = 0; i < g_deferred_count; i++) {
        DeferredText* d = &g_deferred[i];
        float scale = (float)d->font_size / (float)font->glyph_height;
        if (d->bold) {
            const Font* bold_font = font_get_bold();
            if (d->clipped) {
                int cx = d->x;
                int limit_x = d->x + d->max_w;
                const char* t = d->text;
                while (*t) {
                    int cw = font_char_advance(bold_font, *t, scale);
                    if (cx + cw > limit_x) break;
                    font_draw_char(&ctx->fb, bold_font, cx, d->y, *t, d->color, scale);
                    cx += cw;
                    t++;
                }
            } else {
                font_draw_string_bold(&ctx->fb, d->x, d->y, d->text, d->color, scale);
            }
        } else {
            if (d->clipped) {
                int cx = d->x;
                int limit_x = d->x + d->max_w;
                const char* t = d->text;
                while (*t) {
                    int cw = font_char_advance(font, *t, scale);
                    if (cx + cw > limit_x) break;
                    font_draw_char(&ctx->fb, font, cx, d->y, *t, d->color, scale);
                    cx += cw;
                    t++;
                }
            } else {
                font_draw_string(&ctx->fb, font, d->x, d->y, d->text, d->color, scale);
            }
        }
    }
    g_deferred_count = 0;
}

static bool is_cursor_over(AppContext* ctx, int x, int y, int w, int h);

static void draw_rounded_rect(Framebuffer* fb, int x, int y, int w, int h, int r, uint32_t color) {
    fb_fill_rect(fb, x + r, y, w - 2 * r, h, color);
    fb_fill_rect(fb, x, y + r, w, h - 2 * r, color);
    fb_fill_circle(fb, x + r, y + r, r, color);
    fb_fill_circle(fb, x + w - r - 1, y + r, r, color);
    fb_fill_circle(fb, x + r, y + h - r - 1, r, color);
    fb_fill_circle(fb, x + w - r - 1, y + h - r - 1, r, color);
}

static void draw_button_fb(Framebuffer* fb, PlatformWindow window, int x, int y, int w, int h, const char* label, bool hover, bool pressed) {
    uint32_t bg = pressed ? AERO_BTN_PRESS : (hover ? AERO_BTN_HOVER : AERO_TOOLBAR_BG);
    draw_rounded_rect(fb, x, y, w, h, 4, bg);
    fb_draw_rect(fb, x, y, w, h, AERO_BORDER);
    int tw = 0;
    if (label) tw = platform_get_api()->text_measure(label, 13, false);
    if (label && tw > 0)
        deferred_text_draw(x + (w - tw) / 2, y + (h - 13) / 2, label, 13, AERO_TEXT, false);
}

static void draw_toolbar_aero(Framebuffer* fb, AppContext* ctx) {
    int w = ctx->win_w;
    fb_fill_rect(fb, 0, 0, w, AERO_TAB_HEIGHT, AERO_TAB_BG);
    fb_fill_rect(fb, 0, AERO_TAB_HEIGHT, w, AERO_TOOLBAR_HEIGHT, AERO_TOOLBAR_BG);
    fb_fill_rect(fb, 0, AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT - 1, w, 1, AERO_BORDER);

    int tx = 8, ty = 2, th = AERO_TAB_HEIGHT - 4;
    BrowserTab* tab = browser_get_active_tab(&ctx->browser);
    const char* tab_title = (tab && tab->title.data) ? tab->title.data : "New Tab";
    int tw = ctx->api->text_measure(tab_title, 13, false);
    if (tw < 60) tw = 60;
    if (tw > 200) tw = 200;
    fb_fill_rect(fb, tx, ty, tw, th, AERO_ACTIVE_TAB);
    fb_draw_rect(fb, tx, ty, tw, th, AERO_BORDER);
    {
        int cx = tx + tw;
        fb_fill_rect(fb, cx, ty + th - 2, 2, 2, AERO_ACTIVE_TAB);
    }
    deferred_text_draw_clipped(tx + 8, ty + (th - 13) / 2, tw - 28, tab_title, 13, AERO_TEXT, false);
    int cx = tx + tw - 18;
    deferred_text_draw(cx + 2, ty + (th - 13) / 2, "x", 13, 0xFF666666, false);

    int by = AERO_TAB_HEIGHT + 6;
    int bh = AERO_BTN_H;
    int btn_x = 8;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "<", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, ">", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "R", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "H", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;

    int url_x = btn_x + 8;
    int url_y = by;
    int url_w = w - 160 - url_x;
    int url_h = bh;
    draw_rounded_rect(fb, url_x, url_y, url_w, url_h, 4, AERO_URL_BG);
    fb_draw_rect(fb, url_x, url_y, url_w, url_h, ctx->url_bar_focused ? 0xFF3399FF : AERO_BORDER);
    const char* display_url = "";
    if (ctx->url_bar_focused)
        display_url = ctx->url_input;
    else if (tab && tab->url.data)
        display_url = tab->url.data;
    deferred_text_draw_clipped(url_x + 8, url_y + (url_h - 13) / 2, url_w - 16, display_url, 13, AERO_TEXT, false);
    if (ctx->url_bar_focused) {
        int sw = ctx->url_input[0] ? ctx->api->text_measure(ctx->url_input, 13, false) : 0;
        static int sb = 0; sb++;
        if ((sb / 30) % 2)
            fb_fill_rect(fb, url_x + 8 + sw, url_y + 6, 2, 16, 0xFF000000);
    }

    btn_x = w - 160;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "+", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "*", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "v", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "T", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H)); btn_x += AERO_BTN_W + 2;
    draw_button_fb(fb, ctx->window, btn_x, by, AERO_BTN_W, bh, "S", is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H), ctx->mouse_l && is_cursor_over(ctx, btn_x, by, AERO_BTN_W, AERO_BTN_H));
}

static void draw_bookmark_bar(Framebuffer* fb, AppContext* ctx) {
    int y = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT;
    if (!ctx->settings_show_bookmarks) return;
    int w = ctx->win_w;
    fb_fill_rect(fb, 0, y, w, AERO_BOOKMARK_HEIGHT, AERO_TOOLBAR_BG);
    fb_fill_rect(fb, 0, y + AERO_BOOKMARK_HEIGHT - 1, w, 1, AERO_BORDER);
    int bx = 8;
    for (int i = 0; i < ctx->browser.bookmark_count; i++) {
        const char* name = ctx->browser.bookmarks[i].name.data;
        if (!name || !name[0]) continue;
        int nw = ctx->api->text_measure(name, 13, false) + 16;
        if (bx + nw > w - 20) break;
        draw_rounded_rect(fb, bx, y + 4, nw, AERO_BOOKMARK_HEIGHT - 8, 4, AERO_TOOLBAR_BG);
        fb_draw_rect(fb, bx, y + 4, nw, AERO_BOOKMARK_HEIGHT - 8, AERO_BORDER);
        deferred_text_draw(bx + 8, y + (AERO_BOOKMARK_HEIGHT - 13) / 2, name, 13, AERO_LINK, false);
        bx += nw + 4;
    }
}

static void draw_status_bar(Framebuffer* fb, AppContext* ctx) {
    int y = ctx->win_h - AERO_STATUS_HEIGHT;
    int w = ctx->win_w;
    fb_fill_rect(fb, 0, y, w, AERO_STATUS_HEIGHT, AERO_TOOLBAR_BG);
    fb_fill_rect(fb, 0, y, w, 1, AERO_BORDER);
    BrowserTab* tab = browser_get_active_tab(&ctx->browser);
    const char* status = "Ready";
    if (tab) {
        if (tab->state == TAB_STATE_LOADING) status = "Loading...";
        else if (tab->state == TAB_STATE_ERROR) status = "Error";
    }
    deferred_text_draw(8, y + (AERO_STATUS_HEIGHT - 13) / 2, status, 13, 0xFF404040, false);
    if (ctx->platform_state.controller_connected) {
        const char* ctrl = "A:Click B:Back LB/RB:Tabs Y:Refresh X:DL Start:URL Back:Settings";
        int iw = ctx->api->text_measure(ctrl, 11, false);
        deferred_text_draw(ctx->win_w - iw - 8, y + (AERO_STATUS_HEIGHT - 11) / 2, ctrl, 11, 0xFF999999, false);
    }
}

static void start_download(AppContext* ctx, const char* url) {
    if (ctx->download_count >= MAX_DOWNLOADS || !url || !url[0]) return;
    DownloadEntry* dl = &ctx->downloads[ctx->download_count];
    memset(dl, 0, sizeof(DownloadEntry));
    strncpy(dl->url, url, sizeof(dl->url) - 1);
    const char* fname = strrchr(url, '/');
    if (fname) fname++; else fname = "download";
    strncpy(dl->filename, fname, sizeof(dl->filename) - 1);
    dl->client = http_client_create();
    if (dl->client && http_client_request(dl->client, url, HTTP_METHOD_GET)) {
        dl->state = DL_RECEIVING;
        ctx->download_count++;
    } else {
        if (dl->client) http_client_destroy(dl->client);
        dl->client = NULL;
    }
}

static void update_downloads(AppContext* ctx) {
    for (int i = 0; i < ctx->download_count; i++) {
        DownloadEntry* dl = &ctx->downloads[i];
        if (dl->state == DL_RECEIVING && dl->client) {
            http_client_poll(dl->client);
            if (http_client_is_done(dl->client)) {
                HttpResponse* resp = http_client_get_response(dl->client);
                if (resp && resp->status_code == 200 && resp->body.length > 0) {
                    char path[MAX_PATH];
                    _snprintf(path, sizeof(path), "downloads/%s", dl->filename);
                    FILE* f = fopen(path, "wb");
                    if (f) {
                        fwrite(resp->body.data, 1, resp->body.length, f);
                        fclose(f);
                        dl->state = DL_DONE;
                        strncpy(dl->save_path, path, sizeof(dl->save_path) - 1);
                    } else {
                        dl->state = DL_ERROR;
                    }
                    dl->bytes_received = resp->body.length;
                } else {
                    dl->state = DL_ERROR;
                    dl->status_code = resp ? resp->status_code : 0;
                }
                http_client_destroy(dl->client);
                dl->client = NULL;
            }
        }
    }
}

static void draw_download_panel(Framebuffer* fb, AppContext* ctx) {
    if (!ctx->show_downloads) return;
    int pw = 320, ph = 250;
    int px = ctx->win_w - pw - 16;
    int py = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT + 4;
    draw_rounded_rect(fb, px, py, pw, ph, 6, 0xFFFFFFFF);
    fb_draw_rect(fb, px, py, pw, ph, AERO_BORDER);
    deferred_text_draw(px + 8, py + 6, "Downloads", 14, 0xFF333333, true);
    int cy = py + 28;
    if (ctx->download_count == 0) {
        deferred_text_draw(px + 16, cy + 8, "No downloads", 12, 0xFF999999, false);
    } else {
        for (int i = 0; i < ctx->download_count && i < 5; i++) {
            DownloadEntry* dl = &ctx->downloads[i];
            const char* status_text = "...";
            uint32_t status_color = 0xFF999999;
            if (dl->state == DL_DONE) { status_text = "Done"; status_color = 0xFF22AA22; }
            else if (dl->state == DL_ERROR) { status_text = "Error"; status_color = 0xFFFF2222; }
            else if (dl->state == DL_RECEIVING) { status_text = "Receiving..."; status_color = 0xFF3399FF; }
            deferred_text_draw_clipped(px + 8, cy, pw - 80, dl->filename, 12, 0xFF333333, false);
            int sw = ctx->api->text_measure(status_text, 11, false);
            deferred_text_draw(px + pw - sw - 8, cy + 1, status_text, 11, status_color, false);
            if (dl->state == DL_RECEIVING && dl->bytes_received > 0) {
                int bw = pw - 16;
                fb_fill_rect(fb, px + 8, cy + 16, bw, 4, 0xFFE0E0E0);
                fb_fill_rect(fb, px + 8, cy + 16, bw / 2, 4, 0xFF3399FF);
            }
            cy += 36;
        }
    }
}

static bool is_ad_blocked(AppContext* ctx, const char* url) {
    if (!url) return false;
    const char* domain = "";
    const char* scheme = strstr(url, "://");
    if (scheme) {
        domain = scheme + 3;
        const char* slash = strchr(domain, '/');
        static char host_buf[256];
        if (slash) {
            int len = (int)(slash - domain);
            if (len >= (int)sizeof(host_buf)) len = (int)sizeof(host_buf) - 1;
            memcpy(host_buf, domain, len);
            host_buf[len] = '\0';
            domain = host_buf;
        }
    }
    return adblock_should_block(&ctx->browser.adblock, url, domain, false);
}

static void cookie_save_to_file(void) {
    FILE* f = fopen("cookies.txt", "w");
    if (!f) return;
    for (int i = 0; i < g_cookie_count; i++) {
        fprintf(f, "%s\t%s\t%s\t%s\t%d\t%d\n",
            g_cookie_jar[i].domain, g_cookie_jar[i].path,
            g_cookie_jar[i].name, g_cookie_jar[i].value,
            g_cookie_jar[i].http_only ? 1 : 0, g_cookie_jar[i].secure ? 1 : 0);
    }
    fclose(f);
}

static void cookie_load_from_file(void) {
    FILE* f = fopen("cookies.txt", "r");
    if (!f) return;
    g_cookie_count = 0;
    while (g_cookie_count < COOKIE_JAR_SIZE && fscanf(f, "%127s\t%255s\t%127s\t%511s\t%d\t%d",
        g_cookie_jar[g_cookie_count].domain, g_cookie_jar[g_cookie_count].path,
        g_cookie_jar[g_cookie_count].name, g_cookie_jar[g_cookie_count].value,
        (int*)&g_cookie_jar[g_cookie_count].http_only,
        (int*)&g_cookie_jar[g_cookie_count].secure) == 6) {
        g_cookie_count++;
    }
    fclose(f);
}

static void cookie_set(const char* domain, const char* name, const char* value) {
    for (int i = 0; i < g_cookie_count; i++) {
        if (strcmp(g_cookie_jar[i].domain, domain) == 0 && strcmp(g_cookie_jar[i].name, name) == 0) {
            strncpy(g_cookie_jar[i].value, value, 511);
            return;
        }
    }
    if (g_cookie_count < COOKIE_JAR_SIZE) {
        CookieEntry* c = &g_cookie_jar[g_cookie_count++];
        memset(c, 0, sizeof(CookieEntry));
        strncpy(c->domain, domain, 127);
        strncpy(c->name, name, 127);
        strncpy(c->value, value, 511);
        c->path[0] = '/';
    }
    cookie_save_to_file();
}

static void cache_save(const char* url, const char* data, int len) {
    if (len <= 0 || len > 1024 * 1024) return;
    int idx = g_cache_count % CACHE_MAX_ENTRIES;
    if (g_cache_count < CACHE_MAX_ENTRIES) g_cache_count++;
    CacheEntry* e = &g_cache[idx];
    memset(e, 0, sizeof(CacheEntry));
    strncpy(e->url, url, sizeof(e->url) - 1);
    _snprintf(e->path, sizeof(e->path), "cache/%d.dat", idx);
    e->timestamp = (int64_t)time(NULL);
    e->size = len;
    FILE* f = fopen(e->path, "wb");
    if (f) { fwrite(data, 1, len, f); fclose(f); }
}

static char* cache_load(const char* url, int* out_len) {
    for (int i = 0; i < g_cache_count; i++) {
        if (strcmp(g_cache[i].url, url) == 0) {
            int64_t now = (int64_t)time(NULL);
            if (now - g_cache[i].timestamp > 3600) continue;
            FILE* f = fopen(g_cache[i].path, "rb");
            if (!f) continue;
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* buf = (char*)malloc(sz + 1);
            if (buf) {
                fread(buf, 1, sz, f);
                buf[sz] = '\0';
                *out_len = (int)sz;
            }
            fclose(f);
            return buf;
        }
    }
    return NULL;
}

static bool is_cursor_over(AppContext* ctx, int x, int y, int w, int h) {
    return ctx->mouse_x >= x && ctx->mouse_x < x + w && ctx->mouse_y >= y && ctx->mouse_y < y + h;
}

/* Draw an ImageData (RGBA from stbi) into the ARGB framebuffer, contain-fitting it
 * into the given area (scaled down to fit entirely, centered, no cropping). The
 * source bytes from stbi are R,G,B,A; they are read as bytes (endian-safe) and
 * packed into the framebuffer's A8R8G8B8 layout. */
static void draw_image_cover(Framebuffer* fb, ImageData* img,
                             int area_x, int area_y, int area_w, int area_h) {
    if (!img || !img->pixels || area_w <= 0 || area_h <= 0) return;

    float scale_w = (float)area_w / (float)img->width;
    float scale_h = (float)area_h / (float)img->height;
    float scale = scale_w < scale_h ? scale_w : scale_h;

    int draw_w = (int)(img->width * scale);
    int draw_h = (int)(img->height * scale);
    int draw_x = area_x + (area_w - draw_w) / 2;
    int draw_y = area_y + (area_h - draw_h) / 2;

    const unsigned char* src8 = (const unsigned char*)img->pixels;
    uint32_t bg = fb_get_pixel(fb, area_x, area_y);

    for (int py = 0; py < draw_h; py++) {
        int ty = draw_y + py;
        if (ty < 0 || ty >= fb->height) continue;
        int sy = (int)((long long)py * img->height / draw_h);
        if (sy < 0) sy = 0; if (sy >= img->height) sy = img->height - 1;
        for (int px = 0; px < draw_w; px++) {
            int tx = draw_x + px;
            if (tx < 0 || tx >= fb->width) continue;
            int sx = (int)((long long)px * img->width / draw_w);
            if (sx < 0) sx = 0; if (sx >= img->width) sx = img->width - 1;

            int idx = (sy * img->width + sx) * 4;
            uint8_t r = src8[idx + 0];
            uint8_t g = src8[idx + 1];
            uint8_t b = src8[idx + 2];
            uint8_t a = src8[idx + 3];

            uint32_t argb;
            if (a >= 250) {
                argb = ((uint32_t)255 << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            } else if (a > 0) {
                /* simple alpha blend over the area background color */
                uint8_t bg_r = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bg_b = bg & 0xFF;
                uint8_t br = (uint8_t)(((int)r * a + bg_r * (255 - a)) / 255);
                uint8_t bg2 = (uint8_t)(((int)g * a + bg_g * (255 - a)) / 255);
                uint8_t bb = (uint8_t)(((int)b * a + bg_b * (255 - a)) / 255);
                argb = ((uint32_t)255 << 24) | ((uint32_t)br << 16) | ((uint32_t)bg2 << 8) | bb;
            } else {
                continue;
            }
            fb_set_pixel(fb, tx, ty, argb);
        }
    }
}

static int count_html_nodes(HtmlNode* node) {
    int n = 1;
    for (int i = 0; i < node->child_count; i++) n += count_html_nodes(node->children[i]);
    return n;
}

static void draw_debug_hud(Framebuffer* fb, AppContext* ctx, BrowserTab* tab, int content_y, int content_w, int content_h) {
    const PlatformAPI* api = ctx->api;
    char buf[256];
    const char* state_name = "?";
    switch (tab->state) {
    case TAB_STATE_LOADING: state_name = "LOADING"; break;
    case TAB_STATE_LOADED:  state_name = "LOADED"; break;
    case TAB_STATE_ERROR:   state_name = "ERROR"; break;
    case TAB_STATE_BLANK:   state_name = "BLANK"; break;
    case TAB_STATE_IMAGE:   state_name = "IMAGE"; break;
    }
    int doc_nodes = (tab->document && tab->document->root) ? count_html_nodes(tab->document->root) : 0;
    int runs = tab->layout.text_run_count;
    int boxes = tab->layout.box_count;
    int imgs = tab->layout.image_count;

    int dy = content_y + 4;
    fb_fill_rect(fb, 4, content_y, content_w - 8, 120, 0xDDFFFFFF);
    fb_draw_rect(fb, 4, content_y, content_w - 8, 120, 0xFF666666);

    _snprintf(buf, sizeof(buf), "DEBUG  state=%s  status=%d  bodylen=%d  gzip=%d chunked=%d",
              state_name, tab->http_status, tab->body_len, tab->body_gzip, tab->body_chunked);
    deferred_text_draw(10, dy, buf, 13, 0xFF000000, true); dy += 18;

    _snprintf(buf, sizeof(buf), "doc=%d  textruns=%d  boxes=%d  imgs=%d  h=%d  scroll=%d",
              doc_nodes, runs, boxes, imgs, tab->layout.total_height, tab->layout.scroll_offset);
    deferred_text_draw(10, dy, buf, 13, 0xFF000000, false); dy += 18;

    if (tab->body_prefix[0])
        deferred_text_draw_clipped(10, dy, content_w - 20, tab->body_prefix, 12, 0xFF0000CC, false);

    if (tab->url.data)
        deferred_text_draw_clipped(10, dy, content_w - 20, tab->url.data, 12, 0xFF333333, false);
}

static void draw_content_area(Framebuffer* fb, AppContext* ctx) {
    const PlatformAPI* api = ctx->api;
    int content_y = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT + AERO_BOOKMARK_HEIGHT;
    int content_h = ctx->win_h - content_y - AERO_STATUS_HEIGHT;
    int content_w = ctx->win_w;
    fb_fill_rect(fb, 0, content_y, content_w, content_h, AERO_CONTENT_BG);

    BrowserTab* tab = browser_get_active_tab(&ctx->browser);
    if (!tab) return;

    if (!tab->is_home_page) draw_debug_hud(fb, ctx, tab, content_y, content_w, content_h);

    if (tab->is_home_page && tab->document) {
        int cx = content_w / 2;
        int cy;
        if (ctx->home_logo) {
            int logo_w = content_w * 2 / 3;
            int logo_h = content_h * 2 / 3;
            int logo_x = (content_w - logo_w) / 2;
            int logo_y = content_y + 8;
            draw_image_cover(fb, ctx->home_logo, logo_x, logo_y, logo_w, logo_h);
            cy = logo_y + logo_h + 15;
        } else {
            deferred_text_draw(cx - 140, content_y + 60, "NetXbox Browser", 32, 0xFFE94560, true);
            deferred_text_draw(cx - 130, content_y + 105, "Fast browsing for modded Xbox 360", 16, 0xFF808080, false);
            cy = content_y + 145;
        }
        int search_w = 420;
        int search_x = cx - search_w / 2;
        draw_rounded_rect(fb, search_x, cy, search_w, 36, 6, AERO_URL_BG);
        fb_draw_rect(fb, search_x, cy, search_w, 36, ctx->search_focused ? 0xFF3399FF : 0xFF4488CC);
        if (ctx->search_input[0])
            deferred_text_draw_clipped(search_x + 12, cy + 10, search_w - 24, ctx->search_input, 16, AERO_TEXT, false);
        else
            deferred_text_draw(search_x + 12, cy + 10, "Search the web...", 16, 0xFFAAAAAA, false);
        if (ctx->search_focused) {
            int sw = ctx->search_input[0] ? api->text_measure(ctx->search_input, 16, false) : 0;
            static int sb = 0; sb++;
            if ((sb / 30) % 2)
                fb_fill_rect(fb, search_x + 12 + sw, cy + 8, 2, 20, 0xFF000000);
        }
        cy += 50;
        const char* links[] = { "DuckDuckGo", "Wikipedia", "YouTube", "GitHub" };
        int link_ws[4];
        for (int i = 0; i < 4; i++) link_ws[i] = api->text_measure(links[i], 14, false) + 16;
        int total_lw = 0;
        for (int i = 0; i < 4; i++) total_lw += link_ws[i] + 8;
        int lx = cx - total_lw / 2;
        for (int i = 0; i < 4; i++) {
            int lw = link_ws[i];
            draw_rounded_rect(fb, lx, cy, lw, 28, 4, 0xFFF5F5F5);
            fb_draw_rect(fb, lx, cy, lw, 28, AERO_BORDER);
            deferred_text_draw(lx + 8, cy + 4, links[i], 14, AERO_LINK, false);
            int tw = api->text_measure(links[i], 14, false);
            fb_fill_rect(fb, lx + 8, cy + 18, tw, 1, AERO_LINK);
            lx += lw + 8;
        }
        return;
    }

    if (tab->state == TAB_STATE_LOADING) {
        /* In-tab loader page: dark background + header + spinner + progress. */
        fb_fill_rect(fb, 0, content_y, content_w, content_h, 0xFF1A1A2E);

        /* Header */
        deferred_text_draw(content_w / 2 - 60, content_y + 24, "NetXbox", 40, 0xFFE94560, true);
        fb_fill_rect(fb, content_w / 2 - 120, content_y + 74, 240, 2, 0xFF16213E);
        deferred_text_draw(content_w / 2 - 80, content_y + 86, "Browser", 14, 0xFF808080, false);

        int cx = content_w / 2;
        int cy = content_y + content_h / 2 - 20;
        static int frame = 0; frame = (frame + 1) % 120;
        int spin_r = 16;
        for (int i = 0; i < 12; i++) {
            float angle = (float)(i + frame / 10) * 3.14159f / 6.0f;
            int sx = cx + (int)(cosf(angle) * 30) - spin_r / 2;
            int sy = cy + (int)(sinf(angle) * 30) - spin_r / 2;
            int brightness = (i + frame / 10) % 12;
            uint8_t alpha = (uint8_t)(40 + brightness * 18);
            fb_fill_rect(fb, sx, sy, 4, 4, (alpha << 24) | 0xE94560);
        }
        deferred_text_draw(cx - 45, cy + 40, "Loading...", 16, 0xFFC0C0C0, true);
        int bar_w = 200, bar_h = 4;
        int bar_x = cx - bar_w / 2, bar_y = cy + 70;
        draw_rounded_rect(fb, bar_x, bar_y, bar_w, bar_h, 2, 0xFF16213E);
        int progress = (frame * 2) % bar_w;
        if (progress < 10) progress = 10;
        draw_rounded_rect(fb, bar_x, bar_y, progress, bar_h, 2, 0xFFE94560);
        const char* url_text = tab->url.data ? tab->url.data : "";
        deferred_text_draw_clipped(cx - 150, cy + 88, 300, url_text, 11, 0xFF808080, false);
        return;
    }

    if (tab->state == TAB_STATE_ERROR) {
        int cx = content_w / 2;
        int cy = content_y + content_h / 2 - 50;
        fb_fill_rect(fb, cx - 24, cy - 24, 48, 48, 0xFFFF2222);
        deferred_text_draw(cx - 8, cy - 12, "!", 24, 0xFFFFFFFF, true);
        cy += 40;
        deferred_text_draw(cx - 100, cy, "Failed to load page", 20, 0xFF333333, true);
        cy += 30;
        deferred_text_draw(cx - 130, cy, "Check your connection and try again.", 14, 0xFF808080, false);
        cy += 30;
        if (tab->url.data)
            deferred_text_draw_clipped(cx - 150, cy, 300, tab->url.data, 12, 0xFF999999, false);
        int btn_w = 120, btn_h = 30;
        int btn_x = cx - btn_w / 2, btn_y = cy + 40;
        draw_rounded_rect(fb, btn_x, btn_y, btn_w, btn_h, 4, 0xFF3399FF);
        int tw = api->text_measure("Try Again", 14, false);
        deferred_text_draw(btn_x + (btn_w - tw) / 2, btn_y + 7, "Try Again", 14, 0xFFFFFFFF, true);
        return;
    }

    if (tab->state == TAB_STATE_LOADED && tab->document) {
        RenderLayout layout = tab->layout;
        int scroll = layout.scroll_offset;
        int content_bottom = content_y + content_h;

        for (int i = 0; i < layout.text_run_count; i++) {
            RenderTextRun* run = &layout.text_runs[i];
            int ry = run->rect.y + content_y - scroll;
            if (ry + run->rect.height < content_y || ry > content_bottom) continue;
            uint32_t color = AERO_TEXT;
            if (run->is_link) color = AERO_LINK;
            int fs = run->font_size > 0 ? run->font_size : 14;
            deferred_text_draw(run->rect.x, ry, run->text.data, fs, color, run->bold);
            if (run->underline) {
                int tw = api->text_measure(run->text.data, fs, run->bold);
                fb_fill_rect(fb, run->rect.x, ry + fs - 1, tw, 1, color);
            }
        }

        for (int i = 0; i < layout.box_count; i++) {
            RenderBox* box = &layout.boxes[i];
            int by = box->rect.y + content_y - scroll;
            if (by + box->rect.height < content_y || by > content_bottom) continue;
            fb_fill_rect(fb, box->rect.x, by, box->rect.width, box->rect.height, box->border_color);
        }

        for (int i = 0; i < layout.image_count; i++) {
            RenderImage* img = &layout.images[i];
            int iy = img->y + content_y - scroll;
            if (iy + img->height < content_y || iy > content_bottom) continue;
            if (img->loaded && img->pixels) {
                for (int py = 0; py < img->height && iy + py < content_bottom; py++) {
                    if (iy + py < content_y) continue;
                    for (int px = 0; px < img->width && img->x + px < ctx->win_w; px++) {
                        uint32_t pixel = img->pixels[py * img->width + px];
                        if ((pixel >> 24) > 0)
                            fb_set_pixel(fb, img->x + px, iy + py, pixel);
                    }
                }
            } else {
                draw_rounded_rect(fb, img->x, iy, img->width, img->height, 4, 0xFFF0F0F0);
                fb_draw_rect(fb, img->x, iy, img->width, img->height, 0xFFCCCCCC);
                if (img->alt_text && img->alt_text[0]) {
                    int tw = api->text_measure(img->alt_text, 13, false);
                    deferred_text_draw(img->x + (img->width - tw) / 2,
                                   iy + img->height / 2 - 6,
                                   img->alt_text, 13, 0xFF808080, false);
                }
            }
        }

        if (layout.total_height > content_h) {
            int bar_x = content_w - 10, bar_w = 8;
            fb_fill_rect(fb, bar_x, content_y, bar_w, content_h, 0xFFE8E8E8);
            float ratio = (float)content_h / (float)(layout.total_height > 0 ? layout.total_height : 1);
            int thumb_h = (int)(content_h * ratio);
            if (thumb_h < 20) thumb_h = 20;
            int thumb_max = content_h - thumb_h;
            int scroll_max = layout.total_height - content_h;
            int thumb_y = (scroll_max > 0) ? (int)((float)scroll * (float)thumb_max / (float)scroll_max) : 0;
            fb_fill_rect(fb, bar_x, content_y + thumb_y, bar_w, thumb_h, 0xFFB0B8C0);
        }
    }

    if (tab->state == TAB_STATE_IMAGE && tab->image_pixels) {
        int img_w = tab->image_width;
        int img_h = tab->image_height;
        if (img_w > content_w - 40) {
            img_h = (int)((long long)img_h * (content_w - 40) / img_w);
            img_w = content_w - 40;
        }
        if (img_h > content_h - 40) {
            img_w = (int)((long long)img_w * (content_h - 40) / img_h);
            img_h = content_h - 40;
        }
        int img_x = (content_w - img_w) / 2;
        int img_y = content_y + (content_h - img_h) / 2;
        const unsigned char* ip8 = (const unsigned char*)tab->image_pixels;
        for (int py = 0; py < img_h; py++) {
            int sy = (int)((long long)py * tab->image_height / img_h);
            for (int px = 0; px < img_w; px++) {
                int sx = (int)((long long)px * tab->image_width / img_w);
                int idx = (sy * tab->image_width + sx) * 4;
                uint8_t r = ip8[idx + 0];
                uint8_t g = ip8[idx + 1];
                uint8_t b = ip8[idx + 2];
                uint8_t a = ip8[idx + 3];
                if (a == 0) continue;
                if (a >= 250) {
                    fb_set_pixel(fb, img_x + px, img_y + py,
                                 ((uint32_t)255 << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
                } else {
                    int br = ((int)r * a + 255 * (255 - a)) / 255;
                    int bg2 = ((int)g * a + 255 * (255 - a)) / 255;
                    int bb = ((int)b * a + 255 * (255 - a)) / 255;
                    fb_set_pixel(fb, img_x + px, img_y + py,
                                 ((uint32_t)255 << 24) | ((uint32_t)br << 16) | ((uint32_t)bg2 << 8) | bb);
                }
            }
        }
        fb_draw_rect(fb, img_x - 1, img_y - 1, img_w + 2, img_h + 2, 0xFFCCCCCC);
        char info[128];
        _snprintf(info, sizeof(info), "%dx%d - %s", tab->image_width, tab->image_height,
                  tab->content_type[0] ? tab->content_type : "image");
        deferred_text_draw(img_x, img_y + img_h + 6, info, 12, 0xFF808080, false);
    }
}

static void draw_settings_panel(Framebuffer* fb, AppContext* ctx) {
    float anim = ctx->settings_anim;
    if (anim <= 0.001f) return;
    int dim = (int)(0x50 * anim);
    if (dim > 0) fb_fill_rect(fb, 0, 0, ctx->win_w, ctx->win_h, ((uint32_t)dim) << 24);
    int panel_w = 520, panel_h = 560;
    int panel_x = (ctx->win_w - panel_w) / 2;
    int ease = (int)((1.0f - anim) * 60.0f);
    int panel_y = (ctx->win_h - panel_h) / 2 + ease;
    draw_rounded_rect(fb, panel_x, panel_y, panel_w, panel_h, 8, 0xFFFFFFFF);
    fb_draw_rect(fb, panel_x, panel_y, panel_w, panel_h, 0xFF3399FF);

    fb_fill_rect(fb, panel_x, panel_y, panel_w, 36, 0xFF3399FF);
    deferred_text_draw(panel_x + 16, panel_y + 10, "Settings", 18, 0xFFFFFFFF, true);

    const char* pages[] = { "General", "Bookmarks", "History" };
    int page_count = 3;
    int tab_x = panel_x + 140;
    for (int i = 0; i < page_count; i++) {
        int tw = ctx->api->text_measure(pages[i], 13, false) + 16;
        uint32_t bg = (ctx->settings_page == i) ? 0xFF1A6FBF : 0xFF2280D0;
        fb_fill_rect(fb, tab_x, panel_y + 36, tw, 28, bg);
        if (ctx->settings_page == i)
            fb_fill_rect(fb, tab_x, panel_y + 63, tw, 1, 0xFF3399FF);
        deferred_text_draw(tab_x + 8, panel_y + 42, pages[i], 13, 0xFFFFFFFF, ctx->settings_page == i);
        tab_x += tw + 2;
    }
    fb_fill_rect(fb, panel_x, panel_y + 64, panel_w, 1, 0xFF3399FF);

    int lx = panel_x + 24;
    int rw = panel_w - 48;
    int cy = panel_y + 80;
    int content_h = panel_h - 130;

    if (ctx->settings_page == 0) {
        const char* hp_options[] = { "NetXbox Home", "DuckDuckGo", "Wikipedia", "Blank Page" };
        deferred_text_draw(lx, cy, "Home Page:", 14, 0xFF333333, true); cy += 20;
        for (int i = 0; i < 4; i++) {
            uint32_t fg = (ctx->settings_homepage == i) ? 0xFF3399FF : 0xFF666666;
            deferred_text_draw(lx + 12, cy + 7, hp_options[i], 13, fg, false);
            fb_fill_rect(fb, lx, cy + 4, 12, 12, 0xFFFFFFFF);
            fb_draw_rect(fb, lx, cy + 4, 12, 12, 0xFF999999);
            if (ctx->settings_homepage == i)
                fb_fill_rect(fb, lx + 3, cy + 7, 6, 6, 0xFF3399FF);
            cy += 32;
        }
        cy += 8;
        const char* fs_labels[] = { "Small (12px)", "Normal (16px)", "Large (20px)" };
        deferred_text_draw(lx, cy, "Font Size:", 14, 0xFF333333, true); cy += 20;
        for (int i = 0; i < 3; i++) {
            uint32_t fg = (ctx->settings_font_size == (12 + i * 4)) ? 0xFF3399FF : 0xFF666666;
            deferred_text_draw(lx + 12, cy + 7, fs_labels[i], 13, fg, false);
            fb_fill_rect(fb, lx, cy + 4, 12, 12, 0xFFFFFFFF);
            fb_draw_rect(fb, lx, cy + 4, 12, 12, 0xFF999999);
            if (ctx->settings_font_size == (12 + i * 4))
                fb_fill_rect(fb, lx + 3, cy + 7, 6, 6, 0xFF3399FF);
            cy += 28;
        }
        cy += 8;
        deferred_text_draw(lx, cy, "Show Toolbar:", 14, 0xFF333333, true); cy += 20;
        draw_rounded_rect(fb, lx, cy, 40, 20, 10, ctx->settings_show_toolbar ? 0xFF3399FF : 0xFFCCCCCC);
        fb_fill_circle(fb, ctx->settings_show_toolbar ? lx + 30 : lx + 10, cy + 10, 8, 0xFFFFFFFF);
        cy += 32;
        deferred_text_draw(lx, cy, "Show Bookmarks Bar:", 14, 0xFF333333, true); cy += 20;
        draw_rounded_rect(fb, lx, cy, 40, 20, 10, ctx->settings_show_bookmarks ? 0xFF3399FF : 0xFFCCCCCC);
        fb_fill_circle(fb, ctx->settings_show_bookmarks ? lx + 30 : lx + 10, cy + 10, 8, 0xFFFFFFFF);
        cy += 32;
        deferred_text_draw(lx, cy, "Ad Blocker: Active", 14, 0xFF22AA22, true); cy += 20;
        char abuf[64];
        _snprintf(abuf, sizeof(abuf), "%d filters active", ctx->browser.adblock.filter_count);
        deferred_text_draw(lx + 12, cy, abuf, 12, 0xFF808080, false);
        cy += 24;
    } else if (ctx->settings_page == 1) {
        deferred_text_draw(lx, cy, "Bookmarks:", 14, 0xFF333333, true); cy += 24;
        int visible = content_h / 28;
        int start = ctx->settings_list_scroll;
        if (start < 0) start = 0;
        for (int i = start; i < ctx->browser.bookmark_count && (i - start) < visible; i++) {
            const char* name = ctx->browser.bookmarks[i].name.data;
            const char* url = ctx->browser.bookmarks[i].url.data;
            if (name) deferred_text_draw_clipped(lx + 4, cy, rw - 60, name, 13, 0xFF1155CC, false);
            if (url) deferred_text_draw_clipped(lx + 4, cy + 14, rw - 60, url, 11, 0xFF808080, false);
            deferred_text_draw(lx + rw - 40, cy, "Open", 12, 0xFF3399FF, false);
            cy += 28;
        }
        if (ctx->browser.bookmark_count == 0)
            deferred_text_draw(lx + 12, cy, "No bookmarks yet", 12, 0xFF999999, false);
        else {
            char cnt[64];
            _snprintf(cnt, sizeof(cnt), "%d bookmarks", ctx->browser.bookmark_count);
            deferred_text_draw(lx, cy + 20, cnt, 11, 0xFF999999, false);
        }
    } else if (ctx->settings_page == 2) {
        deferred_text_draw(lx, cy, "History:", 14, 0xFF333333, true); cy += 24;
        int visible = content_h / 28;
        int start = ctx->settings_list_scroll;
        if (start < 0) start = 0;
        for (int i = start; i < ctx->browser.history_count && (i - start) < visible; i++) {
            HistoryEntry* he = &ctx->browser.history[ctx->browser.history_count - 1 - i];
            const char* title = he->title.data ? he->title.data : "";
            const char* url = he->url.data ? he->url.data : "";
            if (title[0]) deferred_text_draw_clipped(lx + 4, cy, rw - 60, title, 13, 0xFF333333, false);
            else deferred_text_draw_clipped(lx + 4, cy, rw - 60, url, 13, 0xFF333333, false);
            if (title[0]) deferred_text_draw_clipped(lx + 4, cy + 14, rw - 60, url, 11, 0xFF808080, false);
            deferred_text_draw(lx + rw - 50, cy, "Visit", 12, 0xFF3399FF, false);
            cy += 28;
        }
        if (ctx->browser.history_count == 0)
            deferred_text_draw(lx + 12, cy, "No history yet", 12, 0xFF999999, false);
        else {
            char cnt[64];
            _snprintf(cnt, sizeof(cnt), "%d entries", ctx->browser.history_count);
            deferred_text_draw(lx, cy + 20, cnt, 11, 0xFF999999, false);
        }
    }

    int btn_w = 80, btn_h = 30;
    int btn_x = panel_x + panel_w - btn_w - 16;
    int btn_y = panel_y + panel_h - btn_h - 12;
    draw_rounded_rect(fb, btn_x, btn_y, btn_w, btn_h, 4, 0xFF3399FF);
    int tw = ctx->api->text_measure("Close", 14, false);
    deferred_text_draw(btn_x + (btn_w - tw) / 2, btn_y + 8, "Close", 14, 0xFFFFFFFF, true);
}

void app_init(AppContext* ctx) {
    memset(ctx, 0, sizeof(AppContext));
    ctx->api = platform_get_api();
    if (!ctx->api->init()) return;

    PlatformWindowDesc win_desc = {0};
    win_desc.title = "NetXbox Browser";
    win_desc.width = 1280;
    win_desc.height = 720;
    win_desc.resizable = true;
    win_desc.fullscreen = false;
    ctx->window = ctx->api->window_create(&win_desc);
    if (!ctx->window) return;
    ctx->api->window_show(ctx->window);
    ctx->api->window_get_size(ctx->window, &ctx->win_w, &ctx->win_h);
    fb_init(&ctx->fb, ctx->win_w, ctx->win_h);

    browser_init(&ctx->browser);
    browser_add_bookmark(&ctx->browser, "Mojeek", "https://www.mojeek.com/");
    browser_add_bookmark(&ctx->browser, "Wikipedia", "http://www.wikipedia.org");
    browser_add_bookmark(&ctx->browser, "YouTube", "http://www.youtube.com");
    browser_add_bookmark(&ctx->browser, "GitHub", "http://www.github.com");

    ctx->settings_homepage = 0;
    ctx->settings_font_size = 16;
    ctx->settings_show_toolbar = true;
    ctx->settings_show_bookmarks = true;
    ctx->settings_scroll_speed = 3;

    cookie_load_from_file();
    ctx->home_logo = image_load_from_memory(k_home_logo_png, (int)k_home_logo_png_len);
#ifdef PLATFORM_XBOX360
    _mkdir("cache");
#else
    _mkdir("C:\\Users\\pc\\Documents\\Default Project\\NetXbox\\cache");
#endif

    ctx->running = true;
    ctx->api->log(PLATFORM_LOG_INFO, "NetXbox Browser v1.0 on %s", platform_get_name());
}

static void handle_input(AppContext* ctx) {
    PlatformState* ps = &ctx->platform_state;
    if (ps->keyboard.keys[PLATFORM_KEY_ESCAPE]) { ctx->running = false; return; }

    ctx->prev_mouse_l = ctx->mouse_l;
    ctx->prev_mouse_r = ctx->mouse_r;
    ctx->mouse_l = ps->mouse.left;
    ctx->mouse_r = ps->mouse.right;
    ctx->mouse_x = ps->mouse.x;
    ctx->mouse_y = ps->mouse.y;

    // Software on-screen keyboard: while active, only it consumes input.
    if (ctx->os_kb_active) {
        bool just_clicked = ctx->mouse_l && !ctx->prev_mouse_l;
        if ((ps->buttons_pressed & XINPUT_GAMEPAD_B) ||
            (ps->buttons_pressed & XINPUT_GAMEPAD_BACK)) {
            os_kb_cancel(ctx);
            return;
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_Y) {
            os_kb_type(ctx, ' ');
            return;
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_X) {
            uint64_t nowt = ctx->api->get_ticks();
            uint64_t freq = ctx->api->get_freq();
            static uint64_t last_kb_delete = 0;
            if (last_kb_delete == 0 || ((double)(nowt - last_kb_delete) / (double)freq) > 0.35) {
                last_kb_delete = nowt;
                int len = (int)strlen(ctx->os_kb_text);
                int start = ctx->os_kb_cursor;
                while (start > 0 && (ctx->os_kb_text[start - 1] == ' ' || ctx->os_kb_text[start - 1] == '\t')) start--;
                while (start > 0 && ctx->os_kb_text[start - 1] != ' ' && ctx->os_kb_text[start - 1] != '\t') start--;
                memmove(&ctx->os_kb_text[start], &ctx->os_kb_text[ctx->os_kb_cursor], len - ctx->os_kb_cursor + 1);
                ctx->os_kb_cursor = start;
            }
            return;
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_LEFT_SHOULDER) {
            if (ctx->os_kb_cursor > 0) ctx->os_kb_cursor--;
            return;
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
            int clen = (int)strlen(ctx->os_kb_text);
            if (ctx->os_kb_cursor < clen) ctx->os_kb_cursor++;
            return;
        }
        if (just_clicked) {
            os_kb_handle_click(ctx);
            return;
        }
        if (ps->keyboard.keys[PLATFORM_KEY_ENTER]) {
            os_kb_apply(ctx);
            ps->keyboard.keys[PLATFORM_KEY_ENTER] = 0;
            return;
        }
        return; // ignore everything else while the keyboard is open
    }

    if (ps->mouse.scroll_delta != 0) {
        BrowserTab* tab = browser_get_active_tab(&ctx->browser);
        if (tab) {
            int content_top = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT + AERO_BOOKMARK_HEIGHT;
            int content_h = ctx->win_h - content_top - AERO_STATUS_HEIGHT;
            tab->layout.scroll_offset -= ps->mouse.scroll_delta * 3;
            if (tab->layout.scroll_offset < 0) tab->layout.scroll_offset = 0;
            int max_scroll = tab->layout.total_height - content_h;
            if (max_scroll > 0 && tab->layout.scroll_offset > max_scroll)
                tab->layout.scroll_offset = max_scroll;
        }
    }

    if (ps->controller_connected) {
        float deadzone_f = 8000.0f;
        float lx = (float)ps->thumb_lx / 32767.0f;
        float ly = (float)ps->thumb_ly / 32767.0f;
        if (fabsf(lx) < deadzone_f / 32767.0f) lx = 0;
        if (fabsf(ly) < deadzone_f / 32767.0f) ly = 0;

        float speed = 4.0f;
        if (ps->buttons & XINPUT_GAMEPAD_LEFT_THUMB)
            speed = 2.0f;
        if (ps->right_trigger > 0.3f)
            speed = 1.0f;

        ctx->smooth_x += lx * speed;
        ctx->smooth_y += ly * speed;
        if (ctx->smooth_x < 0) ctx->smooth_x = 0;
        if (ctx->smooth_y < 0) ctx->smooth_y = 0;
        if (ctx->smooth_x >= (float)ctx->win_w) ctx->smooth_x = (float)(ctx->win_w - 1);
        if (ctx->smooth_y >= (float)ctx->win_h) ctx->smooth_y = (float)(ctx->win_h - 1);
        ctx->mouse_x = (int)ctx->smooth_x;
        ctx->mouse_y = (int)ctx->smooth_y;
        ps->mouse.x = ctx->mouse_x;
        ps->mouse.y = ctx->mouse_y;

        float rx = (float)ps->thumb_rx / 32767.0f;
        float ry = (float)ps->thumb_ry / 32767.0f;
        if (fabsf(rx) < deadzone_f / 32767.0f) rx = 0;
        if (fabsf(ry) < deadzone_f / 32767.0f) ry = 0;

        static uint64_t last_scroll_tick = 0;
        static uint64_t last_tab_tick = 0;
        uint64_t now = ctx->api->get_ticks();
        uint64_t freq = ctx->api->get_freq();
        double elapsed = (double)(now - last_scroll_tick) / (double)freq;

        if (fabsf(ry) > 0.3f && elapsed > 0.05) {
            BrowserTab* tab = browser_get_active_tab(&ctx->browser);
            if (tab) {
                int content_h = ctx->win_h - AERO_TAB_HEIGHT - AERO_TOOLBAR_HEIGHT - AERO_BOOKMARK_HEIGHT - AERO_STATUS_HEIGHT;
                tab->layout.scroll_offset += (int)(ry * 30.0f);
                if (tab->layout.scroll_offset < 0) tab->layout.scroll_offset = 0;
                int max_scroll = tab->layout.total_height - content_h;
                if (max_scroll > 0 && tab->layout.scroll_offset > max_scroll)
                    tab->layout.scroll_offset = max_scroll;
                last_scroll_tick = now;
            }
        }

        float lt = ps->left_trigger / 255.0f;
        float rt = ps->right_trigger / 255.0f;
        if (lt > 0.5f && elapsed > 0.05) {
            BrowserTab* tab = browser_get_active_tab(&ctx->browser);
            if (tab) {
                tab->layout.scroll_offset -= ctx->settings_scroll_speed * 3;
                if (tab->layout.scroll_offset < 0) tab->layout.scroll_offset = 0;
                last_scroll_tick = now;
            }
        }
        if (rt > 0.5f && elapsed > 0.05) {
            BrowserTab* tab = browser_get_active_tab(&ctx->browser);
            if (tab) {
                int content_h = ctx->win_h - AERO_TAB_HEIGHT - AERO_TOOLBAR_HEIGHT - AERO_BOOKMARK_HEIGHT - AERO_STATUS_HEIGHT;
                tab->layout.scroll_offset += ctx->settings_scroll_speed * 3;
                int max_scroll = tab->layout.total_height - content_h;
                if (max_scroll > 0 && tab->layout.scroll_offset > max_scroll)
                    tab->layout.scroll_offset = max_scroll;
                if (tab->layout.scroll_offset < 0) tab->layout.scroll_offset = 0;
                last_scroll_tick = now;
            }
        }

        double tab_elapsed = (double)(now - last_tab_tick) / (double)freq;
        if (ps->buttons_pressed & XINPUT_GAMEPAD_LEFT_SHOULDER) {
            if (ctx->browser.tab_count > 0) {
                ctx->browser.active_tab = (ctx->browser.active_tab - 1 + ctx->browser.tab_count) % ctx->browser.tab_count;
                last_tab_tick = now;
            }
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
            if (ctx->browser.tab_count > 0) {
                ctx->browser.active_tab = (ctx->browser.active_tab + 1) % ctx->browser.tab_count;
                last_tab_tick = now;
            }
        }

        if (ps->buttons_pressed & XINPUT_GAMEPAD_A) {
            ps->mouse.left = true;
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_X) {
            os_kb_open(ctx, 1, "");
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_B) {
            if (ctx->show_settings) {
                ctx->show_settings = false;
            } else {
                browser_go_back(&ctx->browser);
            }
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_Y) {
            browser_refresh(&ctx->browser);
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_START) {
            BrowserTab* atab = browser_get_active_tab(&ctx->browser);
            const char* def_url = "";
            if (atab && atab->url.data && atab->url.data[0]) def_url = atab->url.data;
            os_kb_open(ctx, 0, def_url);
            ctx->url_bar_focused = false;
        }
        if (ps->buttons_pressed & XINPUT_GAMEPAD_BACK) {
            /* Always use the software settings panel. The XUI settings scene has
             * checkboxes that don't respond to A on real hardware, so never route
             * settings through XUI chrome. */
            ctx->show_settings = !ctx->show_settings;
        }
    }

    bool just_clicked = ctx->mouse_l && !ctx->prev_mouse_l;
    bool just_right_clicked = ctx->mouse_r && !ctx->prev_mouse_r;
    const PlatformAPI* api = ctx->api;

    if (just_clicked && ctx->show_settings && ctx->settings_anim >= 1.0f) {
        int panel_w = 520, panel_h = 560;
        int panel_x = (ctx->win_w - panel_w) / 2;
        int panel_y = (ctx->win_h - panel_h) / 2;
        int lx = panel_x + 24;
        int rw = panel_w - 48;

        if (ctx->mouse_x >= panel_x && ctx->mouse_x < panel_x + panel_w &&
            ctx->mouse_y >= panel_y && ctx->mouse_y < panel_y + panel_h) {
            /* Close button hit-test (drawn at panel bottom-right). */
            int cb_x = panel_x + panel_w - 80 - 16;
            int cb_y = panel_y + panel_h - 30 - 12;
            if (ctx->mouse_x >= cb_x && ctx->mouse_x < cb_x + 80 &&
                ctx->mouse_y >= cb_y && ctx->mouse_y < cb_y + 30) {
                ctx->show_settings = false;
                return;
            }
            int tab_x = panel_x + 140;
            const char* pages[] = { "General", "Bookmarks", "History" };
            for (int i = 0; i < 3; i++) {
                int tw = ctx->api->text_measure(pages[i], 13, false) + 16;
                if (ctx->mouse_x >= tab_x && ctx->mouse_x < tab_x + tw &&
                    ctx->mouse_y >= panel_y + 36 && ctx->mouse_y < panel_y + 64) {
                    ctx->settings_page = i;
                    ctx->settings_list_scroll = 0;
                }
                tab_x += tw + 2;
            }

            if (ctx->settings_page == 0) {
                int cy = panel_y + 80 + 20;
                for (int i = 0; i < 4; i++) {
                    if (ctx->mouse_x >= lx && ctx->mouse_x < lx + rw &&
                        ctx->mouse_y >= cy && ctx->mouse_y < cy + 28) {
                        ctx->settings_homepage = i;
                    }
                    cy += 32;
                }
                cy += 8;
                cy += 20; /* "Font Size:" label row */
                for (int i = 0; i < 3; i++) {
                    if (ctx->mouse_x >= lx && ctx->mouse_x < lx + rw &&
                        ctx->mouse_y >= cy && ctx->mouse_y < cy + 28) {
                        ctx->settings_font_size = 12 + i * 4;
                    }
                    cy += 28;
                }
                cy += 8;
                cy += 20; /* "Show Toolbar:" label row */
                if (ctx->mouse_x >= lx && ctx->mouse_x < lx + rw &&
                    ctx->mouse_y >= cy && ctx->mouse_y < cy + 20) {
                    ctx->settings_show_toolbar = !ctx->settings_show_toolbar;
                }
                cy += 32;
                cy += 20; /* "Show Bookmarks Bar:" label row */
                if (ctx->mouse_x >= lx && ctx->mouse_x < lx + rw &&
                    ctx->mouse_y >= cy && ctx->mouse_y < cy + 20) {
                    ctx->settings_show_bookmarks = !ctx->settings_show_bookmarks;
                }
            } else if (ctx->settings_page == 1) {
                int cy = panel_y + 80 + 24;
                int visible = (panel_h - 130) / 28;
                int start = ctx->settings_list_scroll;
                if (start < 0) start = 0;
                for (int i = start; i < ctx->browser.bookmark_count && (i - start) < visible; i++) {
                    if (ctx->mouse_x >= lx + rw - 50 && ctx->mouse_x < lx + rw &&
                        ctx->mouse_y >= cy && ctx->mouse_y < cy + 16) {
                        BrowserTab* atab = browser_get_active_tab(&ctx->browser);
                        if (atab) browser_navigate(atab, ctx->browser.bookmarks[i].url.data);
                        ctx->show_settings = false;
                        break;
                    }
                    cy += 28;
                }
            } else if (ctx->settings_page == 2) {
                int cy = panel_y + 80 + 24;
                int visible = (panel_h - 130) / 28;
                int start = ctx->settings_list_scroll;
                if (start < 0) start = 0;
                for (int i = start; i < ctx->browser.history_count && (i - start) < visible; i++) {
                    HistoryEntry* he = &ctx->browser.history[ctx->browser.history_count - 1 - i];
                    if (ctx->mouse_x >= lx + rw - 50 && ctx->mouse_x < lx + rw &&
                        ctx->mouse_y >= cy && ctx->mouse_y < cy + 16) {
                        BrowserTab* atab = browser_get_active_tab(&ctx->browser);
                        if (atab) browser_navigate(atab, he->url.data);
                        ctx->show_settings = false;
                        break;
                    }
                    cy += 28;
                }
            }

            /* A inside the panel only selects/toggles. B is the way to close. */
            return;
        }
        /* A-click outside the panel while settings is open does nothing. */
        return;
    }

    if (just_clicked) {
        int btn_y = AERO_TAB_HEIGHT + 6;
        if (ctx->mouse_y >= btn_y && ctx->mouse_y < btn_y + AERO_BTN_H) {
            if (is_cursor_over(ctx, 8, btn_y, AERO_BTN_W, AERO_BTN_H))
                browser_go_back(&ctx->browser);
            if (is_cursor_over(ctx, 8 + AERO_BTN_W + 2, btn_y, AERO_BTN_W, AERO_BTN_H))
                browser_go_forward(&ctx->browser);
            if (is_cursor_over(ctx, 8 + 2 * (AERO_BTN_W + 2), btn_y, AERO_BTN_W, AERO_BTN_H))
                browser_refresh(&ctx->browser);
            if (is_cursor_over(ctx, 8 + 3 * (AERO_BTN_W + 2), btn_y, AERO_BTN_W, AERO_BTN_H))
                browser_go_home(&ctx->browser);

            int add_x = ctx->win_w - 160;
            if (ctx->mouse_y >= btn_y && ctx->mouse_y < btn_y + AERO_BTN_H) {
                if (ctx->mouse_x >= add_x && ctx->mouse_x < add_x + AERO_BTN_W)
                    browser_create_tab(&ctx->browser, BROWSER_HOME_URL);
                add_x += AERO_BTN_W + 2;
                if (ctx->mouse_x >= add_x && ctx->mouse_x < add_x + AERO_BTN_W) {
                    BrowserTab* tab = browser_get_active_tab(&ctx->browser);
                    if (tab && tab->url.length > 0)
                        browser_add_bookmark(&ctx->browser, tab->title.data, tab->url.data);
                }
                add_x += AERO_BTN_W + 2;
                if (ctx->mouse_x >= add_x && ctx->mouse_x < add_x + AERO_BTN_W) {
                    ctx->show_downloads = !ctx->show_downloads;
                }
                add_x += AERO_BTN_W + 2;
                if (ctx->mouse_x >= add_x && ctx->mouse_x < add_x + AERO_BTN_W) {
                    ctx->url_bar_focused = true;
                    strncpy(ctx->url_input, "http://", sizeof(ctx->url_input) - 1);
                    ctx->url_cursor = (int)strlen(ctx->url_input);
                }
                add_x += AERO_BTN_W + 2;
                if (ctx->mouse_x >= add_x && ctx->mouse_x < add_x + AERO_BTN_W) {
                    ctx->show_settings = !ctx->show_settings;
                }
            }
        }

        for (int i = 0; i < ctx->browser.bookmark_count; i++) {
            int bky = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT;
            if (is_cursor_over(ctx, 0, bky, ctx->win_w, AERO_BOOKMARK_HEIGHT)) {
                const char* bname = ctx->browser.bookmarks[i].name.data;
                if (!bname) continue;
                int bx = 8;
                for (int j = 0; j < i; j++) {
                    const char* n = ctx->browser.bookmarks[j].name.data;
                    if (n) bx += ctx->api->text_measure(n, 13, false) + 20;
                }
                int nw = ctx->api->text_measure(bname, 13, false) + 16;
                if (ctx->mouse_x >= bx && ctx->mouse_x < bx + nw) {
                    browser_navigate(browser_get_active_tab(&ctx->browser), ctx->browser.bookmarks[i].url.data);
                    ctx->url_bar_focused = false;
                    break;
                }
            }
        }

        int content_top = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT + AERO_BOOKMARK_HEIGHT;
        int content_bot = ctx->win_h - AERO_STATUS_HEIGHT;
        if (ctx->mouse_y >= content_top && ctx->mouse_y < content_bot) {
            BrowserTab* tab = browser_get_active_tab(&ctx->browser);
            if (tab && tab->state == TAB_STATE_ERROR) {
                int cx = ctx->win_w / 2;
                int cy = content_top + (content_bot - content_top) / 2 - 10;
                int btn_w = 120, btn_h = 30;
                int btn_x = cx - btn_w / 2, btn_y = cy + 40;
                if (is_cursor_over(ctx, btn_x, btn_y, btn_w, btn_h))
                    browser_refresh(&ctx->browser);
            }
            if (tab && tab->state == TAB_STATE_LOADED && tab->document) {
                RenderLayout layout = tab->layout;
                int scroll = layout.scroll_offset;
                for (int i = 0; i < layout.text_run_count; i++) {
                    RenderTextRun* run = &layout.text_runs[i];
                    if (!run->is_link || !run->link_url.data) continue;
                    int ry = run->rect.y + content_top - scroll;
                    if (ctx->mouse_x >= run->rect.x && ctx->mouse_x < run->rect.x + run->rect.width &&
                        ctx->mouse_y >= ry && ctx->mouse_y < ry + run->rect.height) {
                        browser_navigate(tab, run->link_url.data);
                        break;
                    }
                }
            }
        }

        if (is_cursor_over(ctx, 8, 0, ctx->win_w / 4, AERO_TAB_HEIGHT)) {
            ctx->url_bar_focused = false;
        }

        int url_x = 8 + 4 * (AERO_BTN_W + 2) + 8;
        int url_w = ctx->win_w - 160 - url_x;
        int url_y = AERO_TAB_HEIGHT + 6;
        if (is_cursor_over(ctx, url_x, url_y, url_w, AERO_BTN_H)) {
            BrowserTab* atab = browser_get_active_tab(&ctx->browser);
            const char* def_url = "";
            if (atab && atab->url.data && atab->url.data[0]) def_url = atab->url.data;
            ctx->url_bar_focused = true;
            os_kb_open(ctx, 0, def_url);
        }
    }

    if (just_right_clicked) {
        int content_top = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT + AERO_BOOKMARK_HEIGHT;
        int content_bot = ctx->win_h - AERO_STATUS_HEIGHT;
        if (ctx->mouse_y >= content_top && ctx->mouse_y < content_bot) {
            BrowserTab* tab = browser_get_active_tab(&ctx->browser);
            if (tab && tab->state == TAB_STATE_LOADED && tab->document) {
                RenderLayout layout = tab->layout;
                int scroll = layout.scroll_offset;
                for (int i = 0; i < layout.text_run_count; i++) {
                    RenderTextRun* run = &layout.text_runs[i];
                    if (!run->is_link || !run->link_url.data) continue;
                    int ry = run->rect.y + content_top - scroll;
                    if (ctx->mouse_x >= run->rect.x && ctx->mouse_x < run->rect.x + run->rect.width &&
                        ctx->mouse_y >= ry && ctx->mouse_y < ry + run->rect.height) {
                        start_download(ctx, run->link_url.data);
                        break;
                    }
                }
            }
        }
    }

    if (ctx->url_bar_focused) {
        for (int c = 0; c < ps->char_input.char_count; c++) {
            char ch = (char)ps->char_input.chars[c];
            int len = (int)strlen(ctx->url_input);
            if (len < (int)sizeof(ctx->url_input) - 2) {
                memmove(&ctx->url_input[ctx->url_cursor + 1], &ctx->url_input[ctx->url_cursor], len - ctx->url_cursor + 1);
                ctx->url_input[ctx->url_cursor++] = ch;
            }
        }
        if (ps->keyboard.keys[PLATFORM_KEY_ENTER]) {
            if (ctx->url_input[0]) {
                const char* resolved = browser_resolve_url(&ctx->browser, ctx->url_input);
                BrowserTab* tab = browser_get_active_tab(&ctx->browser);
                if (tab && !is_ad_blocked(ctx, resolved))
                    browser_navigate(tab, resolved);
            }
            ctx->url_bar_focused = false;
            ps->keyboard.keys[PLATFORM_KEY_ENTER] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_BACKSPACE]) {
            int len = (int)strlen(ctx->url_input);
            if (len > 0 && ctx->url_cursor > 0) {
                memmove(&ctx->url_input[ctx->url_cursor - 1], &ctx->url_input[ctx->url_cursor], len - ctx->url_cursor + 1);
                ctx->url_cursor--;
            }
            ps->keyboard.keys[PLATFORM_KEY_BACKSPACE] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_DELETE]) {
            int len = (int)strlen(ctx->url_input);
            if (ctx->url_cursor < len) {
                memmove(&ctx->url_input[ctx->url_cursor], &ctx->url_input[ctx->url_cursor + 1], len - ctx->url_cursor);
            }
            ps->keyboard.keys[PLATFORM_KEY_DELETE] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_LEFT]) {
            if (ctx->url_cursor > 0) ctx->url_cursor--;
            ps->keyboard.keys[PLATFORM_KEY_LEFT] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_RIGHT]) {
            int len = (int)strlen(ctx->url_input);
            if (ctx->url_cursor < len) ctx->url_cursor++;
            ps->keyboard.keys[PLATFORM_KEY_RIGHT] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_HOME]) {
            ctx->url_cursor = 0;
            ps->keyboard.keys[PLATFORM_KEY_HOME] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_END]) {
            ctx->url_cursor = (int)strlen(ctx->url_input);
            ps->keyboard.keys[PLATFORM_KEY_END] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_ESCAPE]) {
            ctx->url_bar_focused = false;
            ps->keyboard.keys[PLATFORM_KEY_ESCAPE] = 0;
        }
    } else if (ctx->search_focused) {
        for (int c = 0; c < ps->char_input.char_count; c++) {
            char ch = (char)ps->char_input.chars[c];
            if (ch == '\r' || ch == '\n') continue;
            int len = (int)strlen(ctx->search_input);
            if (len < (int)sizeof(ctx->search_input) - 2) {
                ctx->search_input[len] = ch;
                ctx->search_input[len + 1] = '\0';
            }
        }
        if (ps->keyboard.keys[PLATFORM_KEY_ENTER]) {
            if (ctx->search_input[0]) {
                char search_url[2048];
                _snprintf(search_url, sizeof(search_url), "https://www.mojeek.com/search?q=%s", ctx->search_input);
                BrowserTab* tab = browser_get_active_tab(&ctx->browser);
                if (tab) browser_navigate(tab, search_url);
            }
            ctx->search_focused = false;
            ps->keyboard.keys[PLATFORM_KEY_ENTER] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_ESCAPE]) {
            ctx->search_focused = false;
            ps->keyboard.keys[PLATFORM_KEY_ESCAPE] = 0;
        } else if (ps->keyboard.keys[PLATFORM_KEY_BACKSPACE]) {
            int len = (int)strlen(ctx->search_input);
            if (len > 0) ctx->search_input[--len] = '\0';
            ps->keyboard.keys[PLATFORM_KEY_BACKSPACE] = 0;
        }
    } else {
        if (ps->keyboard.keys[PLATFORM_KEY_F5]) {
            browser_refresh(&ctx->browser);
            ps->keyboard.keys[PLATFORM_KEY_F5] = 0;
        }
    }

    if (ps->keyboard.keys[PLATFORM_KEY_CONTROL] && ps->keyboard.keys['S']) {
        BrowserTab* tab = browser_get_active_tab(&ctx->browser);
        if (tab && tab->url.length > 0)
            start_download(ctx, tab->url.data);
        ps->keyboard.keys['S'] = 0;
    }

    if (just_clicked) {
        if (is_cursor_over(ctx, 0, ctx->win_h - AERO_STATUS_HEIGHT, ctx->win_w, AERO_STATUS_HEIGHT)) {
            ctx->search_focused = true;
            ctx->search_input[0] = '\0';
            os_kb_open(ctx, 1, "");
        }
    }

    int content_top = AERO_TAB_HEIGHT + AERO_TOOLBAR_HEIGHT + AERO_BOOKMARK_HEIGHT;
    int content_bot = ctx->win_h - AERO_STATUS_HEIGHT;
    if (just_clicked && ctx->mouse_y >= content_top && ctx->mouse_y < content_bot) {
        BrowserTab* tab = browser_get_active_tab(&ctx->browser);
        if (tab && (tab->is_home_page)) {
            int cx = ctx->win_w / 2;
            int search_w = 420;
            int search_x = cx - search_w / 2;
            int search_y = content_top + 145;
            if (is_cursor_over(ctx, search_x, search_y, search_w, 36)) {
                ctx->search_focused = true;
                ctx->search_input[0] = '\0';
                os_kb_open(ctx, 1, "");
            }
            int ly = content_top + 200;
            const char* links[] = { "DuckDuckGo", "Wikipedia", "YouTube", "GitHub" };
            const char* urls[] = { "https://www.mojeek.com/", "http://www.wikipedia.org", "http://www.youtube.com", "http://www.github.com" };
            for (int i = 0; i < 4; i++) {
                int lw = ctx->api->text_measure(links[i], 14, false) + 16;
                if (is_cursor_over(ctx, search_x - 150 + i * (lw + 8), ly, lw, 28)) {
                    browser_navigate(tab, urls[i]);
                    ctx->search_focused = false;
                    break;
                }
            }
        }
    }

    if (!ctx->url_bar_focused && !ctx->search_focused) {
        for (int k = 0; k < 256; k++) {
            if (ps->keyboard.keys[k]) {
                ps->keyboard.keys[k] = 0;
            }
        }
    }
}

static void draw_base_frame(AppContext* ctx) {
    g_deferred_count = 0;
    g_deferred_active = true;
    if (ctx->settings_show_toolbar) draw_toolbar_aero(&ctx->fb, ctx);
    if (ctx->settings_show_bookmarks) draw_bookmark_bar(&ctx->fb, ctx);
    draw_content_area(&ctx->fb, ctx);
    draw_download_panel(&ctx->fb, ctx);
    draw_status_bar(&ctx->fb, ctx);
    g_deferred_active = false;
    flush_deferred_text(ctx);
}

static void snapshot_settings_bg(AppContext* ctx) {
    if (ctx->settings_bg.pixels == NULL ||
        ctx->settings_bg.width != ctx->fb.width ||
        ctx->settings_bg.height != ctx->fb.height) {
        fb_free(&ctx->settings_bg);
        fb_init(&ctx->settings_bg, ctx->fb.width, ctx->fb.height);
    }
    memcpy(ctx->settings_bg.pixels, ctx->fb.pixels,
           (size_t)ctx->fb.width * (size_t)ctx->fb.height * sizeof(uint32_t));
}

static void render_frame(AppContext* ctx) {
    if (ctx->settings_bg.width != ctx->fb.width || ctx->settings_bg.height != ctx->fb.height)
        ctx->settings_bg_valid = false;

    bool settings_open = ctx->settings_anim > 0.001f;

    if (!settings_open) {
        fb_clear(&ctx->fb, AERO_CONTENT_BG);
        draw_base_frame(ctx);
        ctx->settings_bg_valid = false;
    } else if (!ctx->settings_bg_valid) {
        /* First settings frame: render the full base once and cache it so the
         * expensive page/chrome redraw isn't repeated every frame. */
        fb_clear(&ctx->fb, AERO_CONTENT_BG);
        draw_base_frame(ctx);
        snapshot_settings_bg(ctx);
        ctx->settings_bg_valid = true;
        g_deferred_active = true;
        draw_settings_panel(&ctx->fb, ctx);
    } else {
        /* Steady state: restore the cached base (cheap copy), then only the
         * dim overlay + settings panel need to be drawn. */
        memcpy(ctx->fb.pixels, ctx->settings_bg.pixels,
               (size_t)ctx->fb.width * (size_t)ctx->fb.height * sizeof(uint32_t));
        g_deferred_active = true;
        draw_settings_panel(&ctx->fb, ctx);
    }

    /* Always draw the on-screen keyboard overlay and flush pending text. */
    if (ctx->os_kb_active) {
        g_deferred_count = 0;
        g_deferred_active = false;
        os_kb_render(ctx);
    }
    flush_deferred_text(ctx);

    ctx->api->surface_blit(ctx->window, ctx->fb.pixels, ctx->fb.width, ctx->fb.height);
    ctx->api->surface_present(ctx->window);
}

void app_run(AppContext* ctx) {
    const PlatformAPI* api = ctx->api;
    uint64_t last_tick = api->get_ticks();
    uint64_t freq = api->get_freq();

    while (ctx->running) {
        uint64_t now = api->get_ticks();
        double dt = (double)(now - last_tick) / (double)freq;
        last_tick = now;
        if (dt > 0.1) dt = 0.1;

        float target_anim = ctx->show_settings ? 1.0f : 0.0f;
        if (ctx->settings_anim < target_anim) {
            ctx->settings_anim += (float)(dt * 5.0f);
            if (ctx->settings_anim > target_anim) ctx->settings_anim = target_anim;
        } else if (ctx->settings_anim > target_anim) {
            ctx->settings_anim -= (float)(dt * 7.0f);
            if (ctx->settings_anim < target_anim) ctx->settings_anim = target_anim;
        }

        api->poll_events(&ctx->platform_state);
        if (ctx->platform_state.should_close) { ctx->running = false; break; }

        api->window_get_size(ctx->window, &ctx->win_w, &ctx->win_h);
        if (ctx->win_w != ctx->fb.width || ctx->win_h != ctx->fb.height)
            fb_resize(&ctx->fb, ctx->win_w, ctx->win_h);

        ctx->platform_state.window_width = ctx->win_w;
        ctx->platform_state.window_height = ctx->win_h;
        ctx->browser.viewport_width = ctx->win_w;
        ctx->browser.viewport_height = ctx->win_h;

        handle_input(ctx);
        browser_update(&ctx->browser);
        update_downloads(ctx);

        BrowserTab* tab = browser_get_active_tab(&ctx->browser);
        if (tab && tab->state == TAB_STATE_LOADED && tab->document && tab->layout.text_run_count == 0) {
            int content_h = ctx->win_h - AERO_TAB_HEIGHT - AERO_TOOLBAR_HEIGHT - AERO_BOOKMARK_HEIGHT - AERO_STATUS_HEIGHT;
            browser_update_layout(tab, ctx->win_w, content_h);
        }

        render_frame(ctx);

        if (dt < 1.0 / 60.0)
            api->sleep_ms((uint32_t)((1.0 / 60.0 - dt) * 1000.0));
    }
}

void app_shutdown(AppContext* ctx) {
    if (!ctx) return;
    cookie_save_to_file();
    fb_free(&ctx->fb);
    fb_free(&ctx->settings_bg);
    if (ctx->home_logo) image_free(ctx->home_logo);
    ctx->home_logo = NULL;
    browser_shutdown(&ctx->browser);
    if (ctx->window) ctx->api->window_destroy(ctx->window);
    ctx->api->shutdown();
}
