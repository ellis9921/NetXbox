#include "framebuffer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

Color color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    Color c; c.r = r; c.g = g; c.b = b; c.a = 255; return c;
}

Color color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Color c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
}

Color color_hex(uint32_t hex) {
    Color c;
    c.r = (hex >> 16) & 0xFF;
    c.g = (hex >> 8) & 0xFF;
    c.b = hex & 0xFF;
    c.a = (hex >> 24) ? (hex >> 24) & 0xFF : 255;
    return c;
}

Color color_lerp(Color a, Color b, float t) {
    Color c;
    if (t < 0) t = 0; if (t > 1) t = 1;
    c.r = (uint8_t)(a.r + (b.r - a.r) * t);
    c.g = (uint8_t)(a.g + (b.g - a.g) * t);
    c.b = (uint8_t)(a.b + (b.b - a.b) * t);
    c.a = (uint8_t)(a.a + (b.a - a.a) * t);
    return c;
}

uint32_t color_to_u32(Color c) {
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

static inline uint8_t u32_a(uint32_t c) { return (c >> 24) & 0xFF; }
static inline uint8_t u32_r(uint32_t c) { return (c >> 16) & 0xFF; }
static inline uint8_t u32_g(uint32_t c) { return (c >> 8) & 0xFF; }
static inline uint8_t u32_b(uint32_t c) { return c & 0xFF; }

static inline uint32_t make_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static inline uint32_t blend_pixel(uint32_t dst, uint32_t src) {
    uint8_t sa = u32_a(src);
    if (sa == 0) return dst;
    if (sa == 255) return src;
    uint8_t da = u32_a(dst);
    uint32_t ia = sa + (da * (255 - sa)) / 255;
    if (ia == 0) return 0;
    uint32_t r = (u32_r(src) * sa + u32_r(dst) * da * (255 - sa) / 255) / ia;
    uint32_t g = (u32_g(src) * sa + u32_g(dst) * da * (255 - sa) / 255) / ia;
    uint32_t b = (u32_b(src) * sa + u32_b(dst) * da * (255 - sa) / 255) / ia;
    return make_argb((uint8_t)ia, (uint8_t)r, (uint8_t)g, (uint8_t)b);
}

void fb_init(Framebuffer* fb, int width, int height) {
    fb->width = width;
    fb->height = height;
    fb->stride = width;
    fb->pixels = (uint32_t*)calloc(width * height, sizeof(uint32_t));
}

void fb_free(Framebuffer* fb) {
    if (fb->pixels) free(fb->pixels);
    fb->pixels = NULL;
    fb->width = 0;
    fb->height = 0;
}

void fb_resize(Framebuffer* fb, int width, int height) {
    fb_free(fb);
    fb_init(fb, width, height);
}

void fb_clear(Framebuffer* fb, uint32_t color) {
    int count = fb->width * fb->height;
    for (int i = 0; i < count; i++) fb->pixels[i] = color;
}

void fb_set_pixel(Framebuffer* fb, int x, int y, uint32_t color) {
    if (x >= 0 && x < fb->width && y >= 0 && y < fb->height) {
        fb->pixels[y * fb->stride + x] = color;
    }
}

uint32_t fb_get_pixel(Framebuffer* fb, int x, int y) {
    if (x >= 0 && x < fb->width && y >= 0 && y < fb->height) {
        return fb->pixels[y * fb->stride + x];
    }
    return 0;
}

void fb_fill_rect(Framebuffer* fb, int x, int y, int w, int h, uint32_t color) {
    uint8_t sa = u32_a(color);
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= fb->height) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= fb->width) continue;
            if (sa == 255) {
                fb->pixels[j * fb->stride + i] = color;
            } else {
                fb->pixels[j * fb->stride + i] = blend_pixel(fb->pixels[j * fb->stride + i], color);
            }
        }
    }
}

void fb_draw_rect(Framebuffer* fb, int x, int y, int w, int h, uint32_t color) {
    fb_fill_rect(fb, x, y, w, 1, color);
    fb_fill_rect(fb, x, y + h - 1, w, 1, color);
    fb_fill_rect(fb, x, y, 1, h, color);
    fb_fill_rect(fb, x + w - 1, y, 1, h, color);
}

void fb_fill_rect_alpha(Framebuffer* fb, int x, int y, int w, int h, uint32_t color) {
    fb_fill_rect(fb, x, y, w, h, color);
}

void fb_gradient_h(Framebuffer* fb, int x, int y, int w, int h, uint32_t c_left, uint32_t c_right) {
    Color cl = color_hex(c_left);
    Color cr = color_hex(c_right);
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= fb->height) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= fb->width) continue;
            float t = (w > 1) ? (float)(i - x) / (float)(w - 1) : 0;
            Color c = color_lerp(cl, cr, t);
            fb->pixels[j * fb->stride + i] = blend_pixel(fb->pixels[j * fb->stride + i], color_to_u32(c));
        }
    }
}

void fb_gradient_v(Framebuffer* fb, int x, int y, int w, int h, uint32_t c_top, uint32_t c_bot) {
    Color ct = color_hex(c_top);
    Color cb = color_hex(c_bot);
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= fb->height) continue;
        float t = (h > 1) ? (float)(j - y) / (float)(h - 1) : 0;
        Color c = color_lerp(ct, cb, t);
        uint32_t pixel = color_to_u32(c);
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= fb->width) continue;
            fb->pixels[j * fb->stride + i] = blend_pixel(fb->pixels[j * fb->stride + i], pixel);
        }
    }
}

void fb_draw_line(Framebuffer* fb, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_set_pixel(fb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fb_blit(Framebuffer* dst, int dx, int dy, Framebuffer* src, int sx, int sy, int sw, int sh) {
    for (int j = 0; j < sh; j++) {
        int dst_y = dy + j;
        int src_y = sy + j;
        if (dst_y < 0 || dst_y >= dst->height) continue;
        if (src_y < 0 || src_y >= src->height) continue;
        for (int i = 0; i < sw; i++) {
            int dst_x = dx + i;
            int src_x = sx + i;
            if (dst_x < 0 || dst_x >= dst->width) continue;
            if (src_x < 0 || src_x >= src->width) continue;
            uint32_t pixel = src->pixels[src_y * src->stride + src_x];
            if (u32_a(pixel) > 0) {
                dst->pixels[dst_y * dst->stride + dst_x] = blend_pixel(
                    dst->pixels[dst_y * dst->stride + dst_x], pixel);
            }
        }
    }
}

void fb_fill_circle(Framebuffer* fb, int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                fb_set_pixel(fb, cx + x, cy + y, color);
            }
        }
    }
}

void fb_draw_circle(Framebuffer* fb, int cx, int cy, int r, uint32_t color) {
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        fb_set_pixel(fb, cx + x, cy + y, color);
        fb_set_pixel(fb, cx - x, cy + y, color);
        fb_set_pixel(fb, cx + x, cy - y, color);
        fb_set_pixel(fb, cx - x, cy - y, color);
        fb_set_pixel(fb, cx + y, cy + x, color);
        fb_set_pixel(fb, cx - y, cy + x, color);
        fb_set_pixel(fb, cx + y, cy - x, color);
        fb_set_pixel(fb, cx - y, cy - x, color);
        y++;
        if (d < 0) { d += 2 * y + 1; }
        else { x--; d += 2 * (y - x) + 1; }
    }
}
