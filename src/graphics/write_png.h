#pragma once
#include <stdio.h>
#include <png.h>
#include "gfx.h"


/** @brief Writes PNG file using color palette.
  * @param filename path of output file.
  * @param image indexed bitmap */
void WritePng(FILE * outfile, png_byte** image, png_uint_32 width, png_uint_32 height, const struct gfx_palette* pal);
