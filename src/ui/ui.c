#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void ui_input_field_init(UIInputField* field, int x, int y, int width, int height) {
    memset(field, 0, sizeof(UIInputField));
    field->x = x;
    field->y = y;
    field->width = width;
    field->height = height;
    field->cursor_pos = 0;
}

void ui_input_field_update(UIInputField* field, PlatformState* platform) {
    int mx = platform->mouse.x;
    int my = platform->mouse.y;

    field->hovered = (mx >= field->x && mx < field->x + field->width &&
                      my >= field->y && my < field->y + field->height);

    if (platform->mouse.left && field->hovered) {
        field->focused = true;
    } else if (platform->mouse.left && !field->hovered) {
        field->focused = false;
    }

    if (!field->focused) return;

    if (platform->keyboard.keys[PLATFORM_KEY_BACKSPACE] && field->cursor_pos > 0) {
        memmove(field->text + field->cursor_pos - 1, field->text + field->cursor_pos,
                strlen(field->text + field->cursor_pos) + 1);
        field->cursor_pos--;
        platform->keyboard.keys[PLATFORM_KEY_BACKSPACE] = 0;
    }

    if (platform->keyboard.keys[PLATFORM_KEY_DELETE] && field->cursor_pos < (int)strlen(field->text)) {
        memmove(field->text + field->cursor_pos, field->text + field->cursor_pos + 1,
                strlen(field->text + field->cursor_pos + 1) + 1);
        platform->keyboard.keys[PLATFORM_KEY_DELETE] = 0;
    }

    if (platform->keyboard.keys[PLATFORM_KEY_LEFT] && field->cursor_pos > 0) {
        field->cursor_pos--;
        platform->keyboard.keys[PLATFORM_KEY_LEFT] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_RIGHT] && field->cursor_pos < (int)strlen(field->text)) {
        field->cursor_pos++;
        platform->keyboard.keys[PLATFORM_KEY_RIGHT] = 0;
    }

    if (platform->keyboard.keys[PLATFORM_KEY_HOME]) {
        field->cursor_pos = 0;
        platform->keyboard.keys[PLATFORM_KEY_HOME] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_END]) {
        field->cursor_pos = (int)strlen(field->text);
        platform->keyboard.keys[PLATFORM_KEY_END] = 0;
    }

    if (platform->keyboard.keys[PLATFORM_KEY_CONTROL] && platform->keyboard.keys['A']) {
        field->cursor_pos = 0;
        field->selection_start = (int)strlen(field->text);
        platform->keyboard.keys['A'] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_CONTROL] && platform->keyboard.keys['V']) {
        const PlatformAPI* api = platform_get_api();
        char* clip = api->clipboard_get();
        if (clip) {
            int clip_len = (int)strlen(clip);
            int text_len = (int)strlen(field->text);
            int new_len = text_len + clip_len;
            if (new_len < UI_MAX_INPUT_LENGTH - 1) {
                memmove(field->text + field->cursor_pos + clip_len,
                        field->text + field->cursor_pos,
                        text_len - field->cursor_pos + 1);
                memcpy(field->text + field->cursor_pos, clip, clip_len);
                field->cursor_pos += clip_len;
            }
            free(clip);
        }
        platform->keyboard.keys['V'] = 0;
    }
}

void ui_button_init(UIButton* btn, int x, int y, int width, int height, const char* label) {
    memset(btn, 0, sizeof(UIButton));
    btn->x = x;
    btn->y = y;
    btn->width = width;
    btn->height = height;
    btn->label = label;
}

bool ui_button_update(UIButton* btn, PlatformState* platform) {
    int mx = platform->mouse.x;
    int my = platform->mouse.y;

    btn->hovered = (mx >= btn->x && mx < btn->x + btn->width &&
                    my >= btn->y && my < btn->y + btn->height);

    bool clicked = btn->hovered && platform->mouse.left && !platform->mouse.left;
    btn->pressed = btn->hovered && platform->mouse.left;

    return clicked;
}

void ui_init(UIState* ui, BrowserState* browser) {
    memset(ui, 0, sizeof(UIState));
    ui->browser = browser;

    int toolbar_h = 36;
    int url_h = 28;
    int bookmark_h = 28;

    ui->layout.tab_height = toolbar_h;
    ui->layout.toolbar_height = toolbar_h;
    ui->layout.bookmark_bar_height = bookmark_h;

    int y = 0;

    ui_input_field_init(&ui->url_bar, 200, y + 4, browser->viewport_width - 400, url_h);
    y += toolbar_h;

    int bx = 4;
    ui_button_init(&ui->nav_back, bx, 4, 32, toolbar_h - 8, "<"); bx += 36;
    ui_button_init(&ui->nav_forward, bx, 4, 32, toolbar_h - 8, ">"); bx += 36;
    ui_button_init(&ui->nav_refresh, bx, 4, 32, toolbar_h - 8, "R"); bx += 36;
    ui_button_init(&ui->nav_home, bx, 4, 32, toolbar_h - 8, "H"); bx += 36;
    ui_button_init(&ui->btn_new_tab, browser->viewport_width - 100, 4, 40, toolbar_h - 8, "+");
    ui_button_init(&ui->btn_bookmark, browser->viewport_width - 56, 4, 40, toolbar_h - 8, "*");
    ui_button_init(&ui->btn_tabs, browser->viewport_width - 140, 4, 36, toolbar_h - 8, "T");

    browser->ui_bar_height = toolbar_h;
}

static bool just_clicked(bool current, bool prev) {
    return current && !prev;
}

void ui_update(UIState* ui, PlatformState* platform) {
    ui->hover_x = platform->mouse.x;
    ui->hover_y = platform->mouse.y;

    bool was_left = ui->left_click;
    bool was_right = ui->right_click;
    ui->left_click = platform->mouse.left;
    ui->right_click = platform->mouse.right;

    ui_input_field_update(&ui->url_bar, platform);

    if (just_clicked(ui->left_click, was_left)) {
        if (ui->nav_back.hovered) {
            browser_go_back(ui->browser);
        } else if (ui->nav_forward.hovered) {
            browser_go_forward(ui->browser);
        } else if (ui->nav_refresh.hovered) {
            browser_refresh(ui->browser);
        } else if (ui->nav_home.hovered) {
            browser_go_home(ui->browser);
        } else if (ui->btn_new_tab.hovered) {
            browser_create_tab(ui->browser, BROWSER_HOME_URL);
        } else if (ui->btn_bookmark.hovered) {
            BrowserTab* tab = browser_get_active_tab(ui->browser);
            if (tab && tab->url.length > 0) {
                browser_add_bookmark(ui->browser, tab->title.data, tab->url.data);
            }
        } else if (ui->btn_tabs.hovered) {
            ui->tabs_panel_open = !ui->tabs_panel_open;
        }
    }

    if (platform->keyboard.keys[PLATFORM_KEY_CONTROL] && platform->keyboard.keys['T']) {
        browser_create_tab(ui->browser, BROWSER_HOME_URL);
        platform->keyboard.keys['T'] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_CONTROL] && platform->keyboard.keys['W']) {
        browser_close_tab(ui->browser, ui->browser->active_tab);
        platform->keyboard.keys['W'] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_CONTROL] && platform->keyboard.keys['L']) {
        ui->url_bar.focused = true;
        ui->url_bar.cursor_pos = (int)strlen(ui->url_bar.text);
        platform->keyboard.keys['L'] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_F5]) {
        browser_refresh(ui->browser);
        platform->keyboard.keys[PLATFORM_KEY_F5] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_ALT] && platform->keyboard.keys[PLATFORM_KEY_LEFT]) {
        browser_go_back(ui->browser);
        platform->keyboard.keys[PLATFORM_KEY_LEFT] = 0;
    }
    if (platform->keyboard.keys[PLATFORM_KEY_ALT] && platform->keyboard.keys[PLATFORM_KEY_RIGHT]) {
        browser_go_forward(ui->browser);
        platform->keyboard.keys[PLATFORM_KEY_RIGHT] = 0;
    }

    if (platform->keyboard.keys[PLATFORM_KEY_ENTER] && ui->url_bar.focused) {
        const char* url = browser_resolve_url(ui->browser, ui->url_bar.text);
        BrowserTab* tab = browser_get_active_tab(ui->browser);
        if (tab) {
            browser_navigate(tab, url);
        }
        ui->url_bar.focused = false;
        platform->keyboard.keys[PLATFORM_KEY_ENTER] = 0;
    }

    BrowserTab* tab = browser_get_active_tab(ui->browser);
    if (tab && ui->url_bar.text[0] == '\0') {
        strncpy(ui->url_bar.text, tab->url.data ? tab->url.data : "", UI_MAX_INPUT_LENGTH - 1);
        ui->url_bar.cursor_pos = (int)strlen(ui->url_bar.text);
    }

    int content_y = ui->layout.tab_height + ui->layout.toolbar_height;
    if (ui->browser->show_bookmarks_bar) {
        content_y += ui->layout.bookmark_bar_height;
    }

    if (platform->mouse.left && ui->hover_y >= content_y) {
        BrowserTab* scroll_tab = browser_get_active_tab(ui->browser);
        if (scroll_tab && scroll_tab->layout.total_height > ui->browser->viewport_height - content_y) {
            scroll_tab->layout.scroll_offset += 20;
            int max_scroll = scroll_tab->layout.total_height - (ui->browser->viewport_height - content_y);
            if (max_scroll < 0) max_scroll = 0;
            if (scroll_tab->layout.scroll_offset > max_scroll) scroll_tab->layout.scroll_offset = max_scroll;
            if (scroll_tab->layout.scroll_offset < 0) scroll_tab->layout.scroll_offset = 0;
        }
    }

    ui_button_init(&ui->nav_back, 4, 4, 32, 28, "<");
    ui_button_init(&ui->nav_forward, 40, 4, 32, 28, ">");
    ui_button_init(&ui->nav_refresh, 76, 4, 32, 28, "R");
    ui_button_init(&ui->nav_home, 112, 4, 32, 28, "H");
    ui_input_field_init(&ui->url_bar, 156, 4, ui->browser->viewport_width - 310, 28);
    strncpy(ui->url_bar.text, tab && tab->url.data ? tab->url.data : "", UI_MAX_INPUT_LENGTH - 1);
    ui_button_init(&ui->btn_new_tab, ui->browser->viewport_width - 144, 4, 32, 28, "+");
    ui_button_init(&ui->btn_bookmark, ui->browser->viewport_width - 108, 4, 32, 28, "*");
    ui_button_init(&ui->btn_tabs, ui->browser->viewport_width - 72, 4, 32, 28, "T");
}

void ui_draw(UIState* ui) {
    BrowserTab* tab = browser_get_active_tab(ui->browser);

    const PlatformAPI* api = platform_get_api();
    api->log(PLATFORM_LOG_DEBUG, "UI Draw: tabs=%d active=%d url=%s title=%s state=%d",
             ui->browser->tab_count, ui->browser->active_tab,
             tab ? tab->url.data : "none",
             tab ? tab->title.data : "none",
             tab ? tab->state : -1);
}
