#pragma once
#include <stdio.h>
#include <png.h>
#include "gfx.h"


/** @brief Writes PNG file using color palette.
  * @param filename path of output file.
  * @param image indexed bitmap
  * @param pal if NULL function will interpret image as RGBA bitmap */
void gfx_write_png(FILE * outfile, png_byte** image, const uint32_t width, const uint32_t height, const struct gfx_palette *pal);
