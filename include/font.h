#ifndef FONT_H
#define FONT_H

#include "framebuffer.h"
#include <stdint.h>

#include "stb_rect_pack.h"
#include "stb_truetype.h"

#define FONT_ATLAS_SIZE 1024
#define FONT_BAKE_SIZE  96
#define FONT_NUM_CHARS  96
#define FONT_FIRST_CHAR 32

typedef struct {
    const unsigned char* bitmap;
    int bitmap_width;
    int bitmap_height;
    int glyph_width;
    int glyph_height;
    int first_char;
    int num_chars;
    float scale;
    int baked;
} Font;

void  font_init_default(void);
const Font* font_get_default(void);
const Font* font_get_bold(void);
void  font_draw_char(Framebuffer* fb, const Font* font, int x, int y, char c, uint32_t color, float scale);
int   font_draw_string(Framebuffer* fb, const Font* font, int x, int y, const char* text, uint32_t color, float scale);
int   font_draw_string_bold(Framebuffer* fb, int x, int y, const char* text, uint32_t color, float scale);
int   font_char_advance(const Font* font, char c, float scale);
int   font_measure_string(const Font* font, const char* text, float scale);
int   font_measure_string_bold(const char* text, float scale);
int   font_get_height(const Font* font, float scale);

#endif
