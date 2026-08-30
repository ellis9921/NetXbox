#pragma once

#include "platform.h"
#include "html.h"

typedef struct {
    int x, y, width, height;
    int scroll_x, scroll_y;
    bool visible;
} RenderRect;

typedef struct {
    String text;
    RenderRect rect;
    int font_size;
    bool bold;
    bool italic;
    bool underline;
    bool is_link;
    String link_url;
} RenderTextRun;

typedef struct {
    RenderRect rect;
    uint32_t bg_color;
    uint32_t border_color;
    int border_width;
    int border_radius;
} RenderBox;

typedef struct {
    char* href;
    char* alt_text;
    int x, y, width, height;
    bool loaded;
    uint32_t* pixels;
} RenderImage;

typedef struct {
    RenderTextRun* text_runs;
    int text_run_count;
    int text_run_capacity;
    RenderBox* boxes;
    int box_count;
    int box_capacity;
    RenderImage* images;
    int image_count;
    int image_capacity;
    int total_width;
    int total_height;
    int scroll_offset;
} RenderLayout;

typedef struct {
    int font_size;
    uint32_t color;
    uint32_t bg_color;
    bool bold;
    bool italic;
    bool underline;
    char font_family[64];
} RenderStyle;

typedef struct {
    uint32_t r, g, b, a;
} RenderColor;

RenderColor render_color(uint32_t hex);
uint32_t render_color_to_u32(RenderColor c);
RenderLayout render_document(HtmlDocument* doc, int viewport_width, int viewport_height);
void render_layout_free(RenderLayout* layout);
void render_draw_text(void (*draw_func)(int x, int y, const char* text, int font_size, uint32_t color, bool bold), RenderLayout* layout);
void render_draw_rects(void (*draw_func)(int x, int y, int w, int h, uint32_t color), RenderLayout* layout);
