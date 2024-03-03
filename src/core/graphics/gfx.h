#pragma once

#include <png.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../helpers/basicdefs.h"

#define IMG_CHUNKS_LIMIT 32
#define IMG_MAX_WIDTH 2048
#define IMG_MAX_HEIGHT 2048

/**
 * @brief Header containing information about Gex graphic file;
 * Does not contain info about chunks nor operationMapLength (for sprite format);
 * Size: 20 Bytes
 * 
 * @property type_signature
 *  Possible values in BIG ENDIAN :
 *  FF FF XX X0 - bitmap 4 bpp
 *  FF FF XX X1 - bitmap 8 bpp
 *  FF FF XX X2 - bitmap 16 bpp
 *  FF FF XX X4 - sprite 4 bpp
 *  FF FF XX X5 - sprite 8 bpp
 *  FF FF XX X6 - sprite 16 bpp (???) */
struct gex_gfxheader
{
    uint16_t _struct_pad;

    uint32_t inf_img_width;
    uint32_t inf_img_height;
    int32_t bitmap_shift_x;
    int16_t bitmap_shift_y;
    uint32_t type_signature;
};

/**
 * @brief bitmap in Gex can be segmented;
 * gfxChunk struct contains information about one chunk of image;
 * Size: 8 Bytes */
struct gex_gfxchunk
{
    uint16_t start_offset;
    uint8_t width;
    uint8_t height;
    int16_t rel_position_x;
    int16_t rel_position_y;
};

/**
 *  @brief internal palette struct with png tRNS info 
 *  @property tRNS_array array of alpha values (u8) for indexed colors.
 * 0 - full transparency, 255 - full visibility
 * http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.tRNS */
typedef struct gfx_palette
{
    png_color palette[256];
    uint16_t colors_cnt; ///< 16 or 256
    uint8_t tRNS_array[256];
    uint16_t tRNS_count;
} gfx_palette;

/** @brief structure with graphic properties */
typedef struct gfx_properties
{
    uint16_t pos_x;
    uint16_t pos_y;
    bool is_semi_transparent;
    bool is_flipped_horizontally;
    bool is_flipped_vertically;
} gfx_properties;

/** @brief internal bitmap graphic structure */
typedef struct gfx_graphic
{
    uint8_t **bitmap;
    uint32_t width;
    uint32_t height;
    gfx_palette *palette;
    uint32_t palette_offset;
} gfx_graphic;


// --- structures parsing ---
/** @brief parses gfxHeader from input file stream. Shifts stream cursor. Function does not validate data!
    @return pointer to dest or null if failed to read stream */
struct gex_gfxheader *gex_gfxheader_parsef(FILE *ifstream, struct gex_gfxheader *dest);

/** @brief parses gfxChunk from input file stream. Shifts stream cursor. Function does not validate data!
    @return pointer to dest or null if failed to read stream */
struct gex_gfxchunk *gex_gfxchunk_parsef(FILE *ifstream, struct gex_gfxchunk *dest);

struct gex_gfxheader gex_gfxheader_parse_aob(const uint8_t aob[20]);
struct gex_gfxchunk gex_gfxchunk_parse_aob(const uint8_t aob[8]);

/** @brief parses gfx_palette from input file stream. Calls gfx_createPalette. Shifts stream cursor.
    @return pointer to dest or null if failed to read stream or parse */
struct gfx_palette *gfx_palette_parsef(FILE *ifstream, struct gfx_palette *dest);

/** @brief creates palette from gex palette format.
 * gex palette format starts with (LE) 00 XX FF FF for 16 colors or 01 XX FF FF for 256 colors
 * @return struct gfx_palette. Object will have 0 colors if the gexPalette has invalid format */
struct gfx_palette gfx_create_palette(void *gex_palette);

/** @brief converts rgb555 with swapped red and blue channels to rgb 8bpp */
png_color bgr555_to_rgb888(u16 bgr555);

// TODO: FUNCTIONS BELOW DOES NOT CHECK MAX SIZE OF IMG
/** @brief calcuates real size of bitmap (not sprite) from gfx headers
 *  @param gfxHeader array with minimum size of 20 + IMG_CHUNKS_LIMIT or precise precalculated size of headers.
 *  @return size of bitmap data of graphic from its header or 0 if size is invalid. */
size_t gfx_calc_size_of_bitmap(const void *gfx_headers);

/** @brief calcuates real size of sprite from gfx headers
 *  @param gfxHeader array with minimum size of 20 + IMG_CHUNKS_LIMIT or precise precalculated size of headers.
 *  @return size of bitmap data of graphic from its header or 0 if size is invalid. */
size_t gfx_calc_size_of_sprite(const void *gheaders_and_opmap);

/** @brief reads graphic headers from FILE into the dest array. Changes position of file pointer
 *  @param dest address of pointer to which address of allocated array will be assigned. IMPORTANT: must be freed in client function!
 *  @return size of dest array in bytes. 0 if headers are invalid. */
size_t gfx_read_headers_alloc_aob(FILE *gfx_headers_fp, void **dest);

/** @brief detects graphic's type and creates bitmap. calls gfx_draw...
 *  @param gfx_headers pointer to gfxHeader, null terminated array of gfxChunks and, in case of sprite format, operations map. Use gfx_read_headers_alloc_aob if you work with FILE*
 *  @param bitmap_dat pointer to actual image data. IMPORTANT: Use gfx_calc_size_of_bitmap to ensure how many bytes are needed to be read and allocated.
 *  @return image matrix or null pointer if failed */
uint8_t **gfx_draw_img_from_raw(const void *gfx_headers, const uint8_t bitmap_dat[]);

/** @brief detects graphic's type and creates bitmap. calls gfx_draw... gfx_drawImgFromRaw variation, compatible with C/C++, eliminates problem with gfxHeaders size.
 *  @param gfx_header_fp pointer to FILE with position cursor set on graphic header
 *  @param bitmap_dat pointer to actual bitmap. if nullpointer given bitmapDat will be read from the FILE.
 *  @return image matrix or null pointer if failed */
uint8_t **gfx_draw_img_from_rawf(FILE *gfx_header_fp, const uint8_t *bitmap_dat);

/** @brief creates bitmap from PC/PSX 2/4/8 bpp bitmap (LE){ ?, (X0 XX FF FF), (X1 XX FF FF)};
 *  @param chunk_headers pointer to null terminated gex_gfxChunk structs.
 *  @param bitmap_dat pointer to actual bitmap.
 *  @param bpp - bits per pixel 2/4/8
 *  @return pointer to color indexed bitmap.
 *  @return NULL Pointer if failed! */
uint8_t **gfx_draw_gex_bitmap(const void *chunk_headers,
                              const uint8_t bitmap_dat[],
                              uint8_t bpp,
                              uint32_t min_width,
                              uint32_t min_height);

/** @brief creates RGBA bitmap from PC/PSX 16 bpp bitmap (LE X2 XX FF FF);
 *  @param chunk_headers pointer to null terminated gex_gfxChunk structs.
 *  @param bitmap_dat pointer to actual bitmap.
 *  @return pointer to color indexed bitmap.
 *  @return NULL Pointer if failed! */
void **gfx_draw_gex_bitmap_16bpp(const void *chunk_headers,
                                 const void *bitmap_dat,
                                 uint32_t min_width,
                                 uint32_t min_height);

/** @brief creates bitmap from PC/PSX 2/4/8 bpp sprite (LE){ ?, (X4 XX FF FF), (X5 XX FF FF)};
 *  @param chunk_headers_and_opmap pointer to null terminated gex_gfxChunk structs and operations map.
 *  @param bitmapIDat pointer to actual bitmap.
 *  @param bpp - bits per pixel 2/4/8
 *  @return pointer to color indexed bitmap.
 *  @return NULL Pointer if failed! */
uint8_t **gfx_draw_sprite(const void *chunk_headers_and_opmap,
                          const uint8_t bitmap_dat[],
                          uint8_t bpp,
                          uint32_t min_width,
                          uint32_t min_height);

/** @brief calcs real sizes of graphic.
    @return true if sizes are invalid. false if everything is ok.*/
bool gfx_calc_real_width_and_height(uint32_t *ref_width, uint32_t *ref_height, const void *first_chunk);

uint8_t gex_gfxheader_type_get_bpp(uint32_t typeSignature);

void gfx_graphic_close(gfx_graphic *g);

/** @brief combines graphic headers and bitmaps into one graphic. Headers are modified */
void *gfx_combine_graphic_and_bitmaps_w_alloc(const void *gfx, const void **bmps, size_t bmp_n);