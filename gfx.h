#pragma once

#include <png.h>
#include "basicdefs.h"
#include <stdbool.h>

#define IMG_CHUNKS_LIMIT 64
#define IMG_MAX_WIDTH 2048
#define IMG_MAX_HEIGHT 2048

/** @struct gex_gfxHeader
 *  @brief Header containing information about Gex graphic file;
 * does not contain info about chunks nor operationMapLength (for sprite);
 * Size: 20 Bytes
 * 
 * @property gex_GfxHeader::typeSignature
 *  Possible values in BIG ENDIAN :
 *  FF FF 99 80 - bitmap 4 bpp
 *  FF FF 99 81 - bitmap 8 bpp
 *  FF FF 99 84 - sprite 4 bpp
 *  FF FF 99 85 - sprite 8 bpp  */
#pragma pack(push,1)
struct gex_gfxHeader {
    u16 _structPadding; ///<  NULL, no impact on graphic rendering

    u32 inf_imgWidth;  ///< width of canvas, may be different than actual size
    u32 inf_imgHeight; ///< height of canvas, may be different than actual size
    u32 bitmap_shiftX;
    u16 bitmap_shiftY;
    u32 typeSignature;
};
#pragma pack(pop)

/** @struct gex_gfxChunk
 * @brief bitmap in Gex is usually segmented;
 * gfxChunk struct contains information about one chunk of image;
 * Size: 8 Bytes */
#pragma pack(push,1)
struct gex_gfxChunk {
    i16 startPointer; 
    i8 width;
    i8 height;
    i16 rel_positionX;
    i16 rel_positionY;
};
#pragma pack(pop)

/** @struct gfx_palette
 * @brief internal palette struct with png tRNS info */
struct gfx_palette {
    png_color palette[256];
    u16 colorsCount; ///< 16 or 256
    u8 tRNS_array[256]; ///< in practice 0 or 1 values will be used
    u16 tRNS_count;
};


/** @brief creates palette from gex palette format.
 * gex palette format starts with (LE) 00 FF FF FF for 16 colors or 01 FF FF FF for 256 colors
 * @return struct gfx_palette. Object will have 0 colors if the gexPalette has invalid format */
struct gfx_palette gfx_createPalette(void *gexPalette);


/** @brief converts rgb555 with swapped red and blue channels to rgb 8bpp */
png_color bgr555toRgb888(u16 bgr555);


/** @brief detects graphics type and creates bitmap. calls gfx_draw...
 *  @return image matrix or null pointer if failed */
u8** gfx_drawImgFromRaw(void *pointer2Gfx);


/** @brief creates bitmap form PC/PSX 4/8 bpp bitmap (LE){(80 99 FF FF), (81 99 FF FF)};
 * palette is not included
 * @param pointer2Gfx pointer to data without gex_gfxChunk
 * @return two dimensional array of indexed pixels with row pointers;
 * @return NULL Pointer if failed! */
u8** gfx_drawGexBitmap(void *pointer2Gfx, bool is4bpp, u32 minWidth, u32 minHeight);


/** @brief creates bitmap form PC/PSX 8bpp sprite (LE)(85 99 FF FF);
 * palette is not included
 * @param pointer2Gfx pointer to data without gex_gfxChunk
 * @return two dimensional array of indexed pixels with row pointers;
 * @return NULL Pointer if failed! */
u8** gfx_drawSprite(void *pointer2Gfx, bool is4bpp, u32 minWidth, u32 minHeight);


void** malloc2D(u32 y, u32 x, u8 sizeOfElement);