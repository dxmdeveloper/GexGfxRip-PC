#ifndef _GFX_H_
#define _GFX_H_ 1

#include <png.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define IMG_CHUNKS_LIMIT 32
#define IMG_MAX_WIDTH 2048
#define IMG_MAX_HEIGHT 2048


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
    int32_t bitmap_shiftX;
    int16_t bitmap_shiftY;
    uint32_t typeSignature;
};


/** 
 * @brief bitmap in Gex is usually segmented;
 * gfxChunk struct contains information about one chunk of image;
 * Size: 8 Bytes */

struct gex_gfxChunk {
    uint16_t startOffset; 
    uint8_t width;
    uint8_t height;
    int16_t rel_positionX;
    int16_t rel_positionY;
};


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


// --- structures parsing ---
/** @brief parses gfxHeader from input file stream. Shifts stream cursor. Function does not validate data!
    @return pointer to dest or null if failed to read stream */
struct gex_gfxHeader * gex_gfxHeader_parsef(FILE * ifstream, struct gex_gfxHeader * dest);

/** @brief parses gfxChunk from input file stream. Shifts stream cursor. Function does not validate data!
    @return pointer to dest or null if failed to read stream */
struct gex_gfxChunk * gex_gfxChunk_parsef(FILE * ifstream, struct gex_gfxChunk * dest);

struct gex_gfxHeader gex_gfxHeader_parseAOB(const uint8_t aob[20]);
struct gex_gfxChunk gex_gfxChunk_parseAOB(const uint8_t aob[8]);



/** @brief creates palette from gex palette format.
 * gex palette format starts with (LE) 00 XX FF FF for 16 colors or 01 XX FF FF for 256 colors
 * @return struct gfx_palette. Object will have 0 colors if the gexPalette has invalid format */
struct gfx_palette gfx_createPalette(void *gexPalette);


/** @brief converts rgb555 with swapped red and blue channels to rgb 8bpp */
png_color bgr555toRgb888(uint16_t bgr555);


/** @param gfxHeader array with minimum SIZE of 20 + IMG_CHUNKS_LIMIT or precise precalculated size of headers.
 *  @return size of bitmap data of graphic from its header or 0 if size is invalid. */
size_t gfx_checkSizeOfBitmap(const void * gfxHeaders);


//// /** @return size of graphic headers and bitmap data. */
//// size_t gfx_checkTotalSizeOfGfx(void * gfxHeaders);

/** @brief reads graphic headers from FILE into the dest array
 *  @param dest address of pointer to which address of allocated array will be assigned. IMPORTANT: must be freed in client function!
 *  @return size of dest array in bytes. 0 if headers are invalid. */
size_t gex_gfxHeadersFToAOB(FILE * gfxHeadersFile, void ** dest);


/** @brief detects graphic's type and creates bitmap. calls gfx_draw...
 *  @param gfxHeaders pointer to gfxHeader, null terminated array of gfxChunks and, in case of sprite format, operations map. Use gex_gfxHeadersFToAOB if you work with FILE*
 *  @param bitmapDat pointer to actual image data. IMPORTANT: Use gfx_checkSizeOfBitmap to ensure how many bytes are needed to be read and allocated. 
 *  @return image matrix or null pointer if failed */
uint8_t **gfx_drawImgFromRaw(const void *gfxHeaders, const uint8_t bitmapDat[]);

/** @brief detects graphic's type and creates bitmap. calls gfx_draw... gfx_drawImgFromRaw variation, compatibile with C/C++, eliminates problem with gfxHeaders size.
 *  @param gfxHeadersFile pointer to FILE with position cursor set on graphic header 
 *  @param bitmapDat pointer to actual image data. if nullpointer given bitmapDat will be read from the FILE.  
 *  @return image matrix or null pointer if failed */
uint8_t **gfx_drawImgFromRawf(FILE * gfxHeadersFile, const uint8_t * bitmapDat);

/** @brief creates bitmap form PC/PSX 4/8 bpp bitmap (LE){(80 XX FF FF), (81 XX FF FF)};
 *  @param chunksHeaders pointer to null terminated gex_gfxChunk structs.
 *  @param bitmapIDat pointer to actual image data.
 *  @return pointer to color indexed bitmap.
 *  @return NULL Pointer if failed! */
uint8_t **gfx_drawGexBitmap(const void * chunkHeaders, const uint8_t bitmapDat[], bool is4bpp, uint32_t minWidth, uint32_t minHeight);

/** @brief creates bitmap form PC/PSX 4/8 bpp sprite (LE){(84 XX FF FF), (85 XX FF FF)};
 *  @param chunksHeadersAndOpMap pointer to null terminated gex_gfxChunk structs and operations map.
 *  @param bitmapIDat pointer to actual image data.
 *  @return pointer to color indexed bitmap.
 *  @return NULL Pointer if failed! */
uint8_t **gfx_drawSprite(const void *chunksHeadersAndOpMap, const uint8_t bitmapDat[], bool is4bpp, uint32_t minWidth, uint32_t minHeight);

#endif