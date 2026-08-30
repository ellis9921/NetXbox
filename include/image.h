#pragma once

#include <stdint.h>

typedef struct {
    int width;
    int height;
    int channels;
    uint32_t* pixels;
} ImageData;

ImageData* image_load_from_memory(const uint8_t* data, int data_len);
void image_free(ImageData* img);
