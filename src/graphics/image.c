#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "image.h"
#include <stdlib.h>
#include <string.h>

ImageData* image_load_from_memory(const uint8_t* data, int data_len) {
    if (!data || data_len <= 0) return NULL;

    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(data, data_len, &w, &h, &ch, 4);
    if (!pixels || w <= 0 || h <= 0) return NULL;

    ImageData* img = (ImageData*)malloc(sizeof(ImageData));
    if (!img) { stbi_image_free(pixels); return NULL; }

    img->width = w;
    img->height = h;
    img->channels = 4;
    img->pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));
    if (!img->pixels) { free(img); stbi_image_free(pixels); return NULL; }

    memcpy(img->pixels, pixels, w * h * sizeof(uint32_t));
    stbi_image_free(pixels);
    return img;
}

void image_free(ImageData* img) {
    if (!img) return;
    if (img->pixels) free(img->pixels);
    free(img);
}
