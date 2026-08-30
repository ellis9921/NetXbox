#include "font.h"
#include <string.h>
#include <stdlib.h>

#include "font_data.h"

static Font g_font_normal;
static Font g_font_bold_data;
static unsigned char g_font_atlas[FONT_ATLAS_SIZE * FONT_ATLAS_SIZE];
static unsigned char g_font_atlas_bold[FONT_ATLAS_SIZE * FONT_ATLAS_SIZE];
static stbtt_bakedchar g_baked_chars[96];
static stbtt_bakedchar g_baked_chars_bold[96];

void font_init_default(void) {
    memset(&g_font_normal, 0, sizeof(Font));
    memset(&g_font_bold_data, 0, sizeof(Font));

    int atlas_size = FONT_ATLAS_SIZE;

    g_font_normal.glyph_width = 8;
    g_font_normal.glyph_height = FONT_BAKE_SIZE;
    g_font_normal.first_char = 32;
    g_font_normal.num_chars = 95;

    int baked_h = FONT_BAKE_SIZE;
    int ret = stbtt_BakeFontBitmap(TTF_ARIAL, 0, (float)FONT_BAKE_SIZE,
                                   g_font_atlas, atlas_size, atlas_size,
                                   32, 96, g_baked_chars);
    if (ret <= 0) {
        baked_h = 64;
        ret = stbtt_BakeFontBitmap(TTF_ARIAL, 0, 64.0f,
                                   g_font_atlas, atlas_size, atlas_size,
                                   32, 96, g_baked_chars);
    }
    if (ret <= 0) {
        baked_h = 48;
        ret = stbtt_BakeFontBitmap(TTF_ARIAL, 0, 48.0f,
                                   g_font_atlas, atlas_size, atlas_size,
                                   32, 96, g_baked_chars);
    }
    if (ret <= 0) {
        baked_h = 32;
        ret = stbtt_BakeFontBitmap(TTF_ARIAL, 0, 32.0f,
                                   g_font_atlas, atlas_size, atlas_size,
                                   32, 96, g_baked_chars);
    }
    g_font_normal.glyph_height = baked_h;
    g_font_normal.bitmap = g_font_atlas;
    g_font_normal.bitmap_width = atlas_size;
    g_font_normal.bitmap_height = atlas_size;
    g_font_normal.baked = (ret > 0) ? 1 : 0;

    g_font_bold_data.glyph_width = 8;
    g_font_bold_data.glyph_height = FONT_BAKE_SIZE;
    g_font_bold_data.first_char = 32;
    g_font_bold_data.num_chars = 95;

    baked_h = FONT_BAKE_SIZE;
    ret = stbtt_BakeFontBitmap(TTF_ARIAL_BOLD, 0, (float)FONT_BAKE_SIZE,
                               g_font_atlas_bold, atlas_size, atlas_size,
                               32, 96, g_baked_chars_bold);
    if (ret <= 0) {
        baked_h = 64;
        ret = stbtt_BakeFontBitmap(TTF_ARIAL_BOLD, 0, 64.0f,
                                   g_font_atlas_bold, atlas_size, atlas_size,
                                   32, 96, g_baked_chars_bold);
    }
    if (ret <= 0) {
        baked_h = 48;
        ret = stbtt_BakeFontBitmap(TTF_ARIAL_BOLD, 0, 48.0f,
                                   g_font_atlas_bold, atlas_size, atlas_size,
                                   32, 96, g_baked_chars_bold);
    }
    if (ret <= 0) {
        baked_h = 32;
        ret = stbtt_BakeFontBitmap(TTF_ARIAL_BOLD, 0, 32.0f,
                                   g_font_atlas_bold, atlas_size, atlas_size,
                                   32, 96, g_baked_chars_bold);
    }
    g_font_bold_data.glyph_height = baked_h;
    g_font_bold_data.bitmap = g_font_atlas_bold;
    g_font_bold_data.bitmap_width = atlas_size;
    g_font_bold_data.bitmap_height = atlas_size;
    g_font_bold_data.baked = (ret > 0) ? 1 : 0;
}

const Font* font_get_default(void) {
    return &g_font_normal;
}

const Font* font_get_bold(void) {
    return &g_font_bold_data;
}

void font_draw_char(Framebuffer* fb, const Font* font, int x, int y, char c, uint32_t color, float scale) {
    if (!fb || !font || !font->bitmap) return;
    int idx = (int)c - font->first_char;
    if (idx < 0 || idx >= font->num_chars) return;

    if (font->baked) {
        const stbtt_bakedchar* bc = (font == &g_font_bold_data) ? &g_baked_chars_bold[idx] : &g_baked_chars[idx];
        int bw = (int)(bc->x1 - bc->x0);
        int bh = (int)(bc->y1 - bc->y0);
        if (bw <= 0 || bh <= 0) return;

        int dx = x + (int)(bc->xoff * scale);
        int dy = y + (int)(bc->yoff * scale);
        int dw = (int)((float)bw * scale);
        int dh = (int)((float)bh * scale);
        if (dw <= 0 || dh <= 0) return;

        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        for (int gy = 0; gy < dh; gy++) {
            int dst_y = dy + gy;
            if (dst_y < 0 || dst_y >= fb->height) continue;
            float fy = ((float)gy + 0.5f) / scale - 0.5f;
            if (fy < 0) fy = 0;
            if (fy >= (float)bh - 1.0f) fy = (float)bh - 1.0f;
            int sy = (int)fy;
            float w0 = fy - (float)sy;
            for (int gx = 0; gx < dw; gx++) {
                int dst_x = dx + gx;
                if (dst_x < 0 || dst_x >= fb->width) continue;
                float fx = ((float)gx + 0.5f) / scale - 0.5f;
                if (fx < 0) fx = 0;
                if (fx >= (float)bw - 1.0f) fx = (float)bw - 1.0f;
                int sx = (int)fx;
                float u0 = fx - (float)sx;

                int base = (int)bc->y0 + sy;
                int left = (int)bc->x0 + sx;
                int stride = font->bitmap_width;
                unsigned char a00 = font->bitmap[base * stride + left];
                unsigned char a01 = font->bitmap[base * stride + left + 1];
                unsigned char a10 = font->bitmap[(base + 1) * stride + left];
                unsigned char a11 = font->bitmap[(base + 1) * stride + left + 1];

                float top = (a00 * (1.0f - u0)) + (a01 * u0);
                float bot = (a10 * (1.0f - u0)) + (a11 * u0);
                float alpha = (top * (1.0f - w0)) + (bot * w0);
                int ai = (int)(alpha + 0.5f);
                if (ai == 0) continue;

                uint32_t dst = fb->pixels[dst_y * fb->stride + dst_x];
                uint8_t dr = (dst >> 16) & 0xFF;
                uint8_t dg = (dst >> 8) & 0xFF;
                uint8_t db = dst & 0xFF;

                unsigned int inv = 255 - (unsigned int)ai;
                unsigned int nr = (r * ai + dr * inv) / 255;
                unsigned int ng = (g * ai + dg * inv) / 255;
                unsigned int nb = (b * ai + db * inv) / 255;
                fb->pixels[dst_y * fb->stride + dst_x] = (nr << 16) | (ng << 8) | nb;
            }
        }
    } else {
        if (!font->bitmap) return;
        int cols = font->bitmap_width / font->glyph_width;
        int bx = (idx % cols) * font->glyph_width;
        int by = (idx / cols) * font->glyph_height;
        int w = (int)(font->glyph_width * scale);
        int h = (int)(font->glyph_height * scale);

        for (int gy = 0; gy < h; gy++) {
            int src_y = by + (int)(gy / scale);
            if (src_y >= font->bitmap_height) continue;
            int dst_y = y + gy;
            if (dst_y < 0 || dst_y >= fb->height) continue;
            for (int gx = 0; gx < w; gx++) {
                int src_x = bx + (int)(gx / scale);
                if (src_x >= font->bitmap_width) continue;
                int dst_x = x + gx;
                if (dst_x < 0 || dst_x >= fb->width) continue;
                int byte_idx = (src_y * (font->bitmap_width / 8)) + (src_x / 8);
                int bit_idx = 7 - (src_x % 8);
                if (font->bitmap[byte_idx] & (1 << bit_idx)) {
                    fb->pixels[dst_y * fb->stride + dst_x] = color;
                }
            }
        }
    }
}

int font_draw_string(Framebuffer* fb, const Font* font, int x, int y, const char* text, uint32_t color, float scale) {
    if (!fb || !font || !text) return 0;
    int cx = x;
    while (*text) {
        if (*text == '\n') {
            cx = x;
            y += (int)(font->glyph_height * scale) + 2;
        } else if (*text == '\r') {
            cx = x;
        } else {
            font_draw_char(fb, font, cx, y, *text, color, scale);
            if (font->baked) {
                int idx = (int)*text - font->first_char;
                if (idx >= 0 && idx < font->num_chars) {
                    const stbtt_bakedchar* bc = (font == &g_font_bold_data) ? &g_baked_chars_bold[idx] : &g_baked_chars[idx];
                    cx += (int)(bc->xadvance * scale);
                } else {
                    cx += (int)(font->glyph_width * scale);
                }
            } else {
                cx += (int)(font->glyph_width * scale);
            }
        }
        text++;
    }
    return cx - x;
}

int font_draw_string_bold(Framebuffer* fb, int x, int y, const char* text, uint32_t color, float scale) {
    return font_draw_string(fb, font_get_bold(), x, y, text, color, scale);
}

int font_char_advance(const Font* font, char c, float scale) {
    if (!font) return 0;
    int idx = (int)c - font->first_char;
    if (idx < 0 || idx >= font->num_chars) {
        return (int)(font->glyph_width * scale);
    }
    if (font->baked) {
        const stbtt_bakedchar* bc = (font == &g_font_bold_data) ? &g_baked_chars_bold[idx] : &g_baked_chars[idx];
        return (int)(bc->xadvance * scale);
    }
    return (int)(font->glyph_width * scale);
}

int font_measure_string(const Font* font, const char* text, float scale) {
    if (!font || !text) return 0;
    if (font->baked) {
        int cx = 0;
        while (*text) {
            if (*text != '\n' && *text != '\r') {
                int idx = (int)*text - font->first_char;
                if (idx >= 0 && idx < font->num_chars) {
                    const stbtt_bakedchar* bc = (font == &g_font_bold_data) ? &g_baked_chars_bold[idx] : &g_baked_chars[idx];
                    cx += (int)(bc->xadvance * scale);
                } else {
                    cx += (int)(font->glyph_width * scale);
                }
            }
            text++;
        }
        return cx;
    }
    return (int)(strlen(text) * font->glyph_width * scale);
}

int font_measure_string_bold(const char* text, float scale) {
    return font_measure_string(font_get_bold(), text, scale);
}

int font_get_height(const Font* font, float scale) {
    if (!font) return 0;
    return (int)(font->glyph_height * scale);
}
