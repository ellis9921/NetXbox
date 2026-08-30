#pragma once

/* Embedded NetXbox home-page banner logo (logo.png) as raw PNG bytes.
 * Decoded at runtime with image_load_from_memory() so it works on both the
 * Xbox 360 and Windows builds regardless of working directory. */

extern const unsigned char k_home_logo_png[];
extern const unsigned int  k_home_logo_png_len;
