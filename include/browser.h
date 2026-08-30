#pragma once

#include "platform.h"
#include "types.h"
#include "html.h"
#include "renderer.h"
#include "network.h"
#include "adblock.h"

#define BROWSER_MAX_TABS 16
#define BROWSER_MAX_BOOKMARKS 64
#define BROWSER_MAX_HISTORY 256
#define BROWSER_MAX_IMAGE_DOWNLOADS 32
#define BROWSER_HOME_URL "about:home"

typedef struct {
    String url;
    String title;
    time_t timestamp;
} HistoryEntry;

typedef struct {
    String name;
    String url;
} Bookmark;

typedef enum {
    TAB_STATE_LOADING,
    TAB_STATE_LOADED,
    TAB_STATE_ERROR,
    TAB_STATE_BLANK,
    TAB_STATE_IMAGE
} TabState;

typedef struct {
    int id;
    String url;
    String title;
    String search_query;
    TabState state;
    HtmlDocument* document;
    RenderLayout layout;
    HttpClient* http_client;
    int http_status;
    int body_len;
    int body_gzip;
    int body_chunked;
    char body_prefix[80];
    int scroll_x;
    int scroll_y;
    bool is_home_page;
    int image_width;
    int image_height;
    uint32_t* image_pixels;
    char content_type[128];
} BrowserTab;

typedef struct {
    int tab_index;
    int image_index;
    HttpClient* client;
    char url[2048];
} ImageDownload;

typedef struct {
    BrowserTab* tabs;
    int tab_count;
    int active_tab;
    int next_tab_id;

    Bookmark bookmarks[BROWSER_MAX_BOOKMARKS];
    int bookmark_count;

    HistoryEntry history[BROWSER_MAX_HISTORY];
    int history_count;
    int history_pos;

    ImageDownload image_downloads[BROWSER_MAX_IMAGE_DOWNLOADS];
    int image_download_count;

    String status_text;
    String search_engine_url;

    bool show_bookmarks_bar;
    bool show_home_page;

    int viewport_width;
    int viewport_height;
    int ui_bar_height;
    AdblockEngine adblock;
} BrowserState;

void browser_init(BrowserState* state);
void browser_shutdown(BrowserState* state);
void browser_update(BrowserState* state);

BrowserTab* browser_get_active_tab(BrowserState* state);
BrowserTab* browser_create_tab(BrowserState* state, const char* url);
void browser_close_tab(BrowserState* state, int tab_index);
void browser_switch_tab(BrowserState* state, int tab_index);

void browser_navigate(BrowserTab* tab, const char* url);
void browser_go_back(BrowserState* state);
void browser_go_forward(BrowserState* state);
void browser_refresh(BrowserState* state);
void browser_go_home(BrowserState* state);

void browser_add_bookmark(BrowserState* state, const char* name, const char* url);
void browser_remove_bookmark(BrowserState* state, int index);
void browser_add_history(BrowserState* state, const char* url, const char* title);

const char* browser_resolve_url(BrowserState* state, const char* input);
void browser_update_layout(BrowserTab* tab, int viewport_width, int viewport_height);
