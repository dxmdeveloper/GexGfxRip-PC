#pragma once
#include <stdio.h>
#include <png.h>
#include "basicdefs.h"
#include "gfx.h"

/**
 * @param filename path of output file.
 * @param image indexed bitmap;
 * @param palette array of RGB colors. Every color takes 3 bytes (8bpp);
 * @param num_pal number of colors in the palette. MAX_VAL = 256;
 * @param tRNS array of alpha values (u8) for indexed colors.
 * 0 - full transparency, 255 - full visibility
 * http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.tRNS
 * Can be NULL if num_trans equals 0
 * @param num_trans number of transparency values in tRNS MAX VAL = 256
 */
void WritePng(const char filename[], png_byte** image, const u32 width, const u32 height, const struct gfx_palette* pal);
