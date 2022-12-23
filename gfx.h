#pragma once

#include <png.h>
#include <stdint.h>
#include <stdbool.h>

#define IMG_CHUNKS_LIMIT 64
#define IMG_MAX_WIDTH 2048
#define IMG_MAX_HEIGHT 2048

/*
#pragma pack is required for 64 bit build in order to avoid structure paddings
*/
#pragma pack(push,1) 

/** 
 *  @brief Header containing information about Gex graphic file;
 * Does not contain info about chunks nor operationMapLength (for sprite format);
 * Size: 20 Bytes
 * 
 * @property gex_GfxHeader::typeSignature
 *  Possible values in BIG ENDIAN :
 *  FF FF XX X0 - bitmap 4 bpp
 *  FF FF XX X1 - bitmap 8 bpp
 *  FF FF XX X4 - sprite 4 bpp
 *  FF FF XX X5 - sprite 8 bpp  */
struct gex_gfxHeader {
    uint16_t _structPadding; 

    uint32_t inf_imgWidth;  ///< width of canvas, may be different than actual size
    uint32_t inf_imgHeight; ///< height of canvas, may be different than actual size
    uint32_t bitmap_shiftX;
    uint16_t bitmap_shiftY;
    uint32_t typeSignature;
};
#pragma pack(pop) 

/** 
 * @brief bitmap in Gex is usually segmented;
 * gfxChunk struct contains information about one chunk of image;
 * Size: 8 Bytes */
#pragma pack(push,1)
struct gex_gfxChunk {
    uint16_t startPointer; 
    uint8_t width;
    uint8_t height;
    int16_t rel_positionX;
    int16_t rel_positionY;
};
#pragma pack(pop)

/** 
 *  @brief internal palette struct with png tRNS info 
 *  @property tRNS_array array of alpha values (u8) for indexed colors.
 * 0 - full transparency, 255 - full visibility
 * http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.tRNS */
struct gfx_palette {
    png_color palette[256];
    uint16_t colorsCount; ///< 16 or 256
    uint8_t tRNS_array[256]; 
    uint16_t tRNS_count;
};


//
// ---------------- Functions ----------------
//

/** @brief creates palette from gex palette format.
 * gex palette format starts with (LE) 00 XX FF FF for 16 colors or 01 XX FF FF for 256 colors
 * @return struct gfx_palette. Object will have 0 colors if the gexPalette has invalid format */
struct gfx_palette gfx_createPalette(void *gexPalette);


/** @brief converts rgb555 with swapped red and blue channels to rgb 8bpp */
png_color bgr555toRgb888(uint16_t bgr555);


/** @brief detects graphics type and creates bitmap. calls gfx_draw...
 *  @param chunksHeaders pointer to null terminated gex_gfxChunk structs.
 *  @param bitmapIDat pointer to actual image data.
 *  @return image matrix or null pointer if failed */
uint8_t** gfx_drawImgFromRaw(struct gex_gfxChunk *chunksHeaders, uint8_t *bitmapIDat);


/** @brief creates bitmap form PC/PSX 4/8 bpp bitmap (LE){(80 XX FF FF), (81 XX FF FF)};
 *  @param chunksHeaders pointer to null terminated gex_gfxChunk structs.
 *  @param bitmapIDat pointer to actual image data.
 *  @return pointer to color indexed bitmap.
 *  @return NULL Pointer if failed! */
uint8_t** gfx_drawGexBitmap(struct gex_gfxChunk *chunksHeaders, uint8_t *bitmapIDat, bool is4bpp, uint32_t minWidth, uint32_t minHeight);


/** @brief creates bitmap form PC/PSX 4/8 bpp sprite (LE){(84 XX FF FF), (85 XX FF FF)};
 *  @param chunksHeadersAndOpMap pointer to null terminated gex_gfxChunk structs and operations map.
 *  @param bitmapIDat pointer to actual image data.
 *  @return pointer to color indexed bitmap.
 *  @return NULL Pointer if failed! */
uint8_t** gfx_drawSprite(struct gex_gfxChunk *chunksHeadersAndOpMap, uint8_t *bitmapIDat, bool is4bpp, uint32_t minWidth, uint32_t minHeight);