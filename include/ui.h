#pragma once

#include "platform.h"
#include "browser.h"

#define UI_MAX_INPUT_LENGTH 2048

typedef struct {
    int x, y, width, height;
    char text[UI_MAX_INPUT_LENGTH];
    int cursor_pos;
    int selection_start;
    bool focused;
    bool hovered;
} UIInputField;

typedef struct {
    int x, y, width, height;
    const char* label;
    bool hovered;
    bool pressed;
} UIButton;

typedef struct {
    int x, y;
    int width;
    int tab_height;
    int toolbar_height;
    int bookmark_bar_height;
} UILayout;

typedef struct {
    UIInputField url_bar;
    UIButton nav_back;
    UIButton nav_forward;
    UIButton nav_refresh;
    UIButton nav_home;
    UIButton btn_new_tab;
    UIButton btn_bookmark;
    UIButton btn_tabs;

    bool tabs_panel_open;
    bool settings_panel_open;

    UILayout layout;
    BrowserState* browser;

    int hover_x, hover_y;
    bool left_click;
    bool right_click;
    bool prev_left_click;
    int scroll_delta;
    char character_input;
} UIState;

void ui_init(UIState* ui, BrowserState* browser);
void ui_update(UIState* ui, PlatformState* platform);
void ui_draw(UIState* ui);

void ui_input_field_init(UIInputField* field, int x, int y, int width, int height);
void ui_input_field_update(UIInputField* field, PlatformState* platform);
void ui_button_init(UIButton* btn, int x, int y, int width, int height, const char* label);
bool ui_button_update(UIButton* btn, PlatformState* platform);
