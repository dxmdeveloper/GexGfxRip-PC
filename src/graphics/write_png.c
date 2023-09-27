#include "write_png.h"
#include <stdlib.h>

void error_exit(struct png_struct_def *def, const char *msg)
{
    printf("problem occurred while creating PNG: %s", msg);
    exit(0x504E47);
}

void gfx_write_png(FILE *outfile, png_byte **image, const u32 width, const u32 height, const struct gfx_palette *pal)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    int type = pal ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGBA;

    if (pal && pal->colors_cnt == 0)
	pal = NULL;

    //Initialize PNG structures
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, error_exit, NULL);
    info_ptr = png_create_info_struct(png_ptr);

    png_init_io(png_ptr, outfile);

    //Setting IHDR
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, type, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    if (pal) {
	//Setting palette
	png_set_PLTE(png_ptr, info_ptr, pal->palette, pal->colors_cnt);

	//Setting Transparency
	png_set_tRNS(png_ptr, info_ptr, pal->tRNS_array, pal->tRNS_count, NULL);
    }

    //Write Info to file
    png_write_info(png_ptr, info_ptr);

    //Write Data to file
    png_write_image(png_ptr, image);
    ////for(u32 h = 0; h < height; h++){
    ////    png_write_row(png_ptr, (png_const_bytep) image[h]);
    ////}

    //finish
    png_write_end(png_ptr, NULL);

    //clean
    if (info_ptr != NULL)
	png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != NULL)
	png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
}
