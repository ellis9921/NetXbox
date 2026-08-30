#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t* pixels;
    int width;
    int height;
    int stride;
} Framebuffer;

typedef struct {
    int x, y;
    int width, height;
} Rect;

typedef struct {
    int x, y;
} Point;

typedef struct {
    uint32_t r, g, b, a;
} Color;

Color color_rgb(uint8_t r, uint8_t g, uint8_t b);
Color color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
Color color_hex(uint32_t hex);
Color color_lerp(Color a, Color b, float t);
uint32_t color_to_u32(Color c);

void fb_init(Framebuffer* fb, int width, int height);
void fb_free(Framebuffer* fb);
void fb_resize(Framebuffer* fb, int width, int height);
void fb_clear(Framebuffer* fb, uint32_t color);
void fb_set_pixel(Framebuffer* fb, int x, int y, uint32_t color);
uint32_t fb_get_pixel(Framebuffer* fb, int x, int y);
void fb_fill_rect(Framebuffer* fb, int x, int y, int w, int h, uint32_t color);
void fb_draw_rect(Framebuffer* fb, int x, int y, int w, int h, uint32_t color);
void fb_fill_rect_alpha(Framebuffer* fb, int x, int y, int w, int h, uint32_t color);
void fb_gradient_h(Framebuffer* fb, int x, int y, int w, int h, uint32_t c_left, uint32_t c_right);
void fb_gradient_v(Framebuffer* fb, int x, int y, int w, int h, uint32_t c_top, uint32_t c_bottom);
void fb_draw_line(Framebuffer* fb, int x0, int y0, int x1, int y1, uint32_t color);
void fb_blit(Framebuffer* dst, int dx, int dy, Framebuffer* src, int sx, int sy, int sw, int sh);
void fb_fill_circle(Framebuffer* fb, int cx, int cy, int r, uint32_t color);
void fb_draw_circle(Framebuffer* fb, int cx, int cy, int r, uint32_t color);
