#pragma once

#include "platform.h"
#include "browser.h"
#include "ui.h"
#include "framebuffer.h"
#include "image.h"

#define MAX_DOWNLOADS 8
#define DOWNLOAD_PATH_MAX 512

typedef enum {
    DL_IDLE,
    DL_CONNECTING,
    DL_RECEIVING,
    DL_DONE,
    DL_ERROR
} DownloadState;

typedef struct {
    char url[2048];
    char filename[DOWNLOAD_PATH_MAX];
    char save_path[DOWNLOAD_PATH_MAX];
    DownloadState state;
    int status_code;
    int bytes_received;
    int content_length;
    HttpClient* client;
} DownloadEntry;

typedef struct AppContext {
    const PlatformAPI* api;
    PlatformState platform_state;
    BrowserState browser;
    UIState ui;
    PlatformWindow window;
    Framebuffer fb;
    bool running;
    int win_w, win_h;
    int mouse_x, mouse_y;
    float smooth_x, smooth_y;
    bool mouse_l, mouse_r, prev_mouse_l, prev_mouse_r;
    bool url_bar_focused;
    char url_input[2048];
    int url_cursor;
    bool search_focused;
    char search_input[2048];
    int search_cursor;
    // Software on-screen keyboard overlay (drawn in-app; no XUI dependency).
    bool os_kb_active;
    int  os_kb_target;      // 0 = URL bar, 1 = search
    bool os_kb_shift;
    char os_kb_text[2048];
    int  os_kb_cursor;
    int hover_cursor;
    DownloadEntry downloads[MAX_DOWNLOADS];
    int download_count;
    bool show_downloads;
    bool show_settings;
    float settings_anim;
    bool chrome_ok;              // true if the last XUI chrome session actually ran
    int settings_page;
    int settings_homepage;
    int settings_font_size;
    bool settings_show_toolbar;
    bool settings_show_bookmarks;
    int settings_scroll_speed;
    int settings_scroll_offset;
    int settings_list_scroll;
    Framebuffer settings_bg;    // cached base frame while the settings panel is open
    bool settings_bg_valid;
    ImageData* home_logo;       // decoded home-page banner logo
} AppContext;

void app_init(AppContext* ctx);
void app_run(AppContext* ctx);
void app_shutdown(AppContext* ctx);
