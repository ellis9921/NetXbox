#include "renderer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

RenderColor render_color(uint32_t hex) {
    RenderColor c;
    c.r = (hex >> 16) & 0xFF;
    c.g = (hex >> 8) & 0xFF;
    c.b = hex & 0xFF;
    c.a = 255;
    return c;
}

uint32_t render_color_to_u32(RenderColor c) {
    return (c.a << 24) | (c.r << 16) | (c.g << 8) | c.b;
}

static uint32_t parse_css_color(const char* str) {
    if (!str) return 0xFFFFFFFF;
    if (str[0] == '#') {
        unsigned int hex = 0;
        const char* p = str + 1;
        while (*p) {
            char c = *p;
            if (c >= '0' && c <= '9') hex = hex * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') hex = hex * 16 + (10 + c - 'a');
            else if (c >= 'A' && c <= 'F') hex = hex * 16 + (10 + c - 'A');
            else break;
            p++;
        }
        if (p - str == 4) {
            int r = ((hex >> 8) & 0xF) * 17;
            int g = ((hex >> 4) & 0xF) * 17;
            int b = (hex & 0xF) * 17;
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        return 0xFF000000 | hex;
    }
    if (strcmp(str, "black") == 0) return 0xFF000000;
    if (strcmp(str, "white") == 0) return 0xFFFFFFFF;
    if (strcmp(str, "red") == 0) return 0xFFFF0000;
    if (strcmp(str, "green") == 0) return 0xFF00FF00;
    if (strcmp(str, "blue") == 0) return 0xFF0000FF;
    if (strcmp(str, "gray") == 0 || strcmp(str, "grey") == 0) return 0xFF808080;
    if (strcmp(str, "silver") == 0) return 0xFFC0C0C0;
    if (strcmp(str, "yellow") == 0) return 0xFFFFFF00;
    if (strcmp(str, "transparent") == 0) return 0x00000000;
    return 0xFFFFFFFF;
}

static int get_default_font_size(HtmlTag tag) {
    switch (tag) {
    case HTML_TAG_H1: return 32;
    case HTML_TAG_H2: return 28;
    case HTML_TAG_H3: return 24;
    case HTML_TAG_H4: return 20;
    case HTML_TAG_H5: return 18;
    case HTML_TAG_H6: return 16;
    case HTML_TAG_SMALL: return 12;
    default: return 16;
    }
}

static void layout_node(HtmlNode* node, RenderLayout* layout, int x, int y, int max_width, RenderStyle inherited) {
    if (!node) return;

    int cursor_x = x;
    int cursor_y = y;

    switch (node->type) {
    case HTML_NODE_TEXT: {
        if (node->text.length == 0) break;
        RenderTextRun run = {0};
        run.text = string_clone(node->text);
        run.rect.x = cursor_x;
        run.rect.y = cursor_y;
        run.rect.width = node->text.length * (inherited.font_size / 2 + 1);
        run.rect.height = inherited.font_size + 4;
        run.font_size = inherited.font_size;
        run.bold = inherited.bold;
        run.italic = inherited.italic;
        run.underline = inherited.underline;
        run.is_link = false;

        if (layout->text_run_count >= layout->text_run_capacity) {
            int new_cap = layout->text_run_capacity == 0 ? 16 : layout->text_run_capacity * 2;
            layout->text_runs = (RenderTextRun*)realloc(layout->text_runs, new_cap * sizeof(RenderTextRun));
            layout->text_run_capacity = new_cap;
        }
        layout->text_runs[layout->text_run_count++] = run;

        int text_width = run.rect.width;
        if (cursor_x + text_width > x + max_width) {
            cursor_y += inherited.font_size + 8;
            cursor_x = x;
        }
        layout->total_width = cursor_x + text_width > layout->total_width ? cursor_x + text_width : layout->total_width;
        layout->total_height = cursor_y + run.rect.height > layout->total_height ? cursor_y + run.rect.height : layout->total_height;
        break;
    }
    case HTML_NODE_ELEMENT: {
        RenderStyle style = inherited;
        int block_x = x;
        bool is_block = (node->tag == HTML_TAG_DIV || node->tag == HTML_TAG_P ||
                        node->tag == HTML_TAG_H1 || node->tag == HTML_TAG_H2 ||
                        node->tag == HTML_TAG_H3 || node->tag == HTML_TAG_H4 ||
                        node->tag == HTML_TAG_H5 || node->tag == HTML_TAG_H6 ||
                        node->tag == HTML_TAG_UL || node->tag == HTML_TAG_OL ||
                        node->tag == HTML_TAG_LI || node->tag == HTML_TAG_HEADER ||
                        node->tag == HTML_TAG_FOOTER || node->tag == HTML_TAG_NAV ||
                        node->tag == HTML_TAG_MAIN || node->tag == HTML_TAG_ARTICLE ||
                        node->tag == HTML_TAG_SECTION || node->tag == HTML_TAG_BLOCKQUOTE ||
                        node->tag == HTML_TAG_PRE || node->tag == HTML_TAG_TABLE);

        if (node->tag == HTML_TAG_H1 || node->tag == HTML_TAG_H2 || node->tag == HTML_TAG_H3 ||
            node->tag == HTML_TAG_H4 || node->tag == HTML_TAG_H5 || node->tag == HTML_TAG_H6) {
            style.bold = true;
            style.font_size = get_default_font_size(node->tag);
        }

        if (node->tag == HTML_TAG_STRONG || node->tag == HTML_TAG_B) style.bold = true;
        if (node->tag == HTML_TAG_EM || node->tag == HTML_TAG_I) style.italic = true;
        if (node->tag == HTML_TAG_U || node->tag == HTML_TAG_INS) style.underline = true;
        if (node->tag == HTML_TAG_SMALL) style.font_size = 12;

        if (node->tag == HTML_TAG_A) {
            style.color = 0xFF0066CC;
            style.underline = true;
        }

        const char* color_attr = html_node_get_attribute(node, "color");
        if (color_attr) style.color = parse_css_color(color_attr);

        const char* style_attr = html_node_get_attribute(node, "style");
        if (style_attr) {
            if (strstr(style_attr, "font-weight: bold")) style.bold = true;
            if (strstr(style_attr, "text-decoration: underline")) style.underline = true;
            const char* color_pos = strstr(style_attr, "color:");
            if (color_pos) {
                char color_buf[64] = {0};
                color_pos += 6;
                while (*color_pos == ' ') color_pos++;
                int ci = 0;
                while (*color_pos && *color_pos != ';' && ci < 63) { color_buf[ci++] = *color_pos++; }
                color_buf[ci] = '\0';
                style.color = parse_css_color(color_buf);
            }
            const char* bg_pos = strstr(style_attr, "background-color:");
            if (bg_pos) {
                char bg_buf[64] = {0};
                bg_pos += 17;
                while (*bg_pos == ' ') bg_pos++;
                int bi = 0;
                while (*bg_pos && *bg_pos != ';' && bi < 63) { bg_buf[bi++] = *bg_pos++; }
                bg_buf[bi] = '\0';
                style.bg_color = parse_css_color(bg_buf);
            }
        }

        if (node->tag == HTML_TAG_BODY) {
            const char* bg = html_node_get_attribute(node, "bgcolor");
            if (bg) style.bg_color = parse_css_color(bg);
        }

        if (node->tag == HTML_TAG_BR) {
            cursor_y += inherited.font_size + 8;
            cursor_x = x;
            break;
        }

        if (node->tag == HTML_TAG_HR) {
            RenderBox box = {0};
            box.rect.x = x;
            box.rect.y = cursor_y;
            box.rect.width = max_width;
            box.rect.height = 2;
            box.border_color = 0xFF808080;
            box.border_width = 1;
            if (layout->box_count >= layout->box_capacity) {
                int new_cap = layout->box_capacity == 0 ? 16 : layout->box_capacity * 2;
                layout->boxes = (RenderBox*)realloc(layout->boxes, new_cap * sizeof(RenderBox));
                layout->box_capacity = new_cap;
            }
            layout->boxes[layout->box_count++] = box;
            cursor_y += 10;
            break;
        }

        if (is_block && layout->box_count > 0) {
            cursor_y += style.font_size + 8;
            cursor_x = x;
        }

        if (node->tag == HTML_TAG_IMG) {
            const char* src = html_node_get_attribute(node, "src");
            const char* alt = html_node_get_attribute(node, "alt");
            int w = 300, h = 200;
            const char* w_attr = html_node_get_attribute(node, "width");
            const char* h_attr = html_node_get_attribute(node, "height");
            if (w_attr) w = atoi(w_attr);
            if (h_attr) h = atoi(h_attr);

            RenderImage img = {0};
            img.href = src ? _strdup(src) : NULL;
            img.alt_text = alt ? _strdup(alt) : NULL;
            img.x = cursor_x;
            img.y = cursor_y;
            img.width = w;
            img.height = h;
            img.loaded = false;

            if (layout->image_count >= layout->image_capacity) {
                int new_cap = layout->image_capacity == 0 ? 16 : layout->image_capacity * 2;
                layout->images = (RenderImage*)realloc(layout->images, new_cap * sizeof(RenderImage));
                layout->image_capacity = new_cap;
            }
            layout->images[layout->image_count++] = img;

            cursor_x += w;
            layout->total_width = cursor_x > layout->total_width ? cursor_x : layout->total_width;
            layout->total_height = cursor_y + h > layout->total_height ? cursor_y + h : layout->total_height;
            break;
        }

        for (int i = 0; i < node->child_count; i++) {
            layout_node(node->children[i], layout, cursor_x, cursor_y, max_width, style);
            if (is_block) {
                cursor_y = layout->total_height;
                cursor_x = x;
            }
        }
        break;
    }
    default:
        break;
    }
}

RenderLayout render_document(HtmlDocument* doc, int viewport_width, int viewport_height) {
    RenderLayout layout = {0};
    if (!doc || !doc->root) return layout;

    RenderStyle base_style = {0};
    base_style.font_size = 16;
    base_style.color = 0xFF000000;
    base_style.bg_color = 0xFFFFFFFF;
    strcpy(base_style.font_family, "sans-serif");

    layout_node(doc->root, &layout, 10, 10, viewport_width - 20, base_style);
    return layout;
}

void render_layout_free(RenderLayout* layout) {
    if (!layout) return;
    for (int i = 0; i < layout->text_run_count; i++) {
        string_free(&layout->text_runs[i].text);
        string_free(&layout->text_runs[i].link_url);
    }
    if (layout->text_runs) free(layout->text_runs);
    if (layout->boxes) free(layout->boxes);
    for (int i = 0; i < layout->image_count; i++) {
        if (layout->images[i].href) free(layout->images[i].href);
        if (layout->images[i].alt_text) free(layout->images[i].alt_text);
        if (layout->images[i].pixels) free(layout->images[i].pixels);
    }
    if (layout->images) free(layout->images);
    memset(layout, 0, sizeof(RenderLayout));
}

void render_draw_text(void (*draw_func)(int x, int y, const char* text, int font_size, uint32_t color, bool bold), RenderLayout* layout) {
    if (!draw_func || !layout) return;
    for (int i = 0; i < layout->text_run_count; i++) {
        RenderTextRun* run = &layout->text_runs[i];
        if (run->text.length > 0) {
            draw_func(run->rect.x, run->rect.y - layout->scroll_offset, run->text.data,
                     run->font_size, 0xFF000000, run->bold);
        }
    }
}

void render_draw_rects(void (*draw_func)(int x, int y, int w, int h, uint32_t color), RenderLayout* layout) {
    if (!draw_func || !layout) return;
    for (int i = 0; i < layout->box_count; i++) {
        RenderBox* box = &layout->boxes[i];
        draw_func(box->rect.x, box->rect.y - layout->scroll_offset,
                 box->rect.width, box->rect.height, box->border_color);
    }
}
