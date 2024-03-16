#include "gfx.h"
#include <png.h>
#include "../helpers/basicdefs.h"
#include "../helpers/binary_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

//* STATIC DECLARATIONS:
/// @param cpix_i index of pixel in chunk
static void p_chunk_rel_draw_pixel(u8 **img, const struct gex_gfxchunk *chunk, u16 cpix_i, u8 pix_val, u8 bpp);
static void **calloc2D(u32 y, u32 x, u8 element_size);
static void **malloc2D(u32 y, u32 x, u8 element_size);

//* EXTERN DEFINITIONS:
//
struct gfx_palette gfx_create_palette(void *gex_palette)
{
    struct gfx_palette newPalette = {0};
    u16 *colors = (u16 *) (gex_palette + 4);
    newPalette.tRNS_count = 0;

    //exception: null pointer
    if (!gex_palette) {
        newPalette.colors_cnt = 0;
        return newPalette;
    }

    switch (aob_read_LE_U32(gex_palette) | 0xff00) {
        case 0xffffff00: newPalette.colors_cnt = 16;
            break;
        case 0xffffff01: newPalette.colors_cnt = 256;
            break;
        default: //0xffffff02 is a placeholder for 16 bpp bitmap
            return newPalette;
    }

    //palette creation
    for (u16 i = 0; i < newPalette.colors_cnt; i++) {
        newPalette.palette[i] = bgr555_to_rgb888(colors[i]);
        //transparency
        if (colors[i] == 0) {
            newPalette.tRNS_count = i + 1;
            newPalette.tRNS_array[i] = 0;
        } else {
            newPalette.tRNS_array[i] = 0xFF;
        }
    }

    return newPalette;
}

/*---- structures deserialization ----*/
struct gex_gfxheader *gex_gfxheader_parsef(FILE *ifstream, struct gex_gfxheader *dest)
{
    fread_LE_U16(&dest->_struct_pad, 1, ifstream);
    fread_LE_U32(&dest->inf_img_width, 1, ifstream);
    fread_LE_U32(&dest->inf_img_height, 1, ifstream);
    fread_LE_I32(&dest->bitmap_shift_x, 1, ifstream);
    fread_LE_I16(&dest->bitmap_shift_y, 1, ifstream);
    if (!fread_LE_U32(&dest->type_signature, 1, ifstream))
        return NULL;

    return dest;
}
struct gex_gfxchunk *gex_gfxchunk_parsef(FILE *ifstream, struct gex_gfxchunk *dest)
{
    fread_LE_U16(&dest->start_offset, 1, ifstream);
    fread(&dest->width, sizeof(u8), 1, ifstream);
    fread(&dest->height, sizeof(u8), 1, ifstream);
    fread_LE_I16(&dest->rel_position_x, 1, ifstream);
    if (!fread_LE_I16(&dest->rel_position_y, 1, ifstream))
        return NULL;

    return dest;
}

struct gex_gfxheader gex_gfxheader_parse_aob(const uint8_t aob[20])
{
    struct gex_gfxheader headerSt = {0};
    headerSt._struct_pad = aob_read_LE_U16(aob);
    headerSt.inf_img_width = aob_read_LE_U32(aob + 2);
    headerSt.inf_img_height = aob_read_LE_U32(aob + 6);
    headerSt.bitmap_shift_x = aob_read_LE_I32(aob + 10);
    headerSt.bitmap_shift_y = aob_read_LE_I16(aob + 14);
    headerSt.type_signature = aob_read_LE_U32(aob + 16);
    return headerSt;
}
struct gex_gfxchunk gex_gfxchunk_parse_aob(const uint8_t aob[8])
{
    struct gex_gfxchunk chunkSt = {0};
    chunkSt.start_offset = aob_read_LE_U16(aob);
    chunkSt.width = aob[2];
    chunkSt.height = aob[3];
    chunkSt.rel_position_x = aob_read_LE_I16(aob + 4);
    chunkSt.rel_position_y = aob_read_LE_I16(aob + 6);

    return chunkSt;
}

struct gfx_palette *gfx_palette_parsef(FILE *ifstream, struct gfx_palette *dest)
{
    void *pal_dat = NULL;
    size_t pal_size = 0;
    u32 type = 0;

    if (!fread_LE_U32(&type, 1, ifstream))
        return NULL;
    pal_size = 4 + (type & 1 ? 256 : 16) * 2;
    pal_dat = malloc(pal_size);
    fseek(ifstream, -4, SEEK_CUR);
    if (fread(pal_dat, 1, pal_size, ifstream) < pal_size) {
        free(pal_dat);
        return NULL;
    }

    *dest = gfx_create_palette(pal_dat);
    free(pal_dat);

    if(dest->colors_cnt == 0)
        return NULL;
    return dest;
}

size_t gfx_calc_size_of_bitmap(const void *gfx_headers)
{
    size_t size = 0;
    struct gex_gfxheader header = gex_gfxheader_parse_aob(gfx_headers);
    struct gex_gfxchunk gchunk = gex_gfxchunk_parse_aob(gfx_headers + 20);
    u8 bpp = gex_gfxheader_type_get_bpp(header.type_signature);

    for (uint i = 1; gchunk.start_offset; i++) {
        size += gchunk.width * gchunk.height;
        gchunk = gex_gfxchunk_parse_aob(gfx_headers + 20 + 8 * i);
    }

    if (bpp >= 8)
        return size * (bpp / 8);
    u32 modulo = size % (8 / bpp);
    return size / (8 / bpp) + modulo;
}

size_t gfx_calc_size_of_sprite(const void *gheaders_and_opmap)
{
    struct gex_gfxheader header = gex_gfxheader_parse_aob(gheaders_and_opmap);
    struct gex_gfxchunk gchunk = gex_gfxchunk_parse_aob(gheaders_and_opmap + 20);
    u8 bpp = gex_gfxheader_type_get_bpp(header.type_signature);
    const u8 *opmap = NULL;
    size_t chunk_cnt = 0;
    size_t bytes = 0;
    u32 opmap_size = 0;

    while (gchunk.start_offset) {
        chunk_cnt++;
        gchunk = gex_gfxchunk_parse_aob(gheaders_and_opmap + 20 + chunk_cnt * 8);
    }

    opmap = gheaders_and_opmap + 20 + (chunk_cnt + 1) * 8 + 4;
    opmap_size = aob_read_LE_U32(opmap - 4);
    if (opmap_size == 0)
        return 0;

    for (u32 i = 0; i < opmap_size - 4; i++) {
        if (opmap[i] < 0x80)
            bytes += (opmap[i] == 0 ? 4096 / bpp : opmap[i] * 32 / bpp);
        else
            bytes += bpp / 2;
    }

    return bytes;
}

gfx_graphic gfx_draw_img_from_raw(const void *gfx_headers, const uint8_t bitmap_dat[])
{
    struct gex_gfxheader header = {0};
    u32 real_height = 0, real_width = 0;
    u8 bpp = 0; // TODO: USE IT
    struct gfx_graphic empty_ret = {0};

    if (gfx_headers == NULL || bitmap_dat == NULL)
        return empty_ret;

    header = gex_gfxheader_parse_aob((u8 *) gfx_headers);
    gfx_headers += 20;

    //if(header.inf_imgWidth > IMG_MAX_WIDTH || header.inf_imgHeight > IMG_MAX_HEIGHT) return NULL;

    gfx_calc_real_width_and_height(&real_height, &real_width, gfx_headers);

    switch (header.type_signature & 7) {
        case 5: return gfx_draw_sprite(gfx_headers, bitmap_dat, 8, real_height, real_width);
        case 4: return gfx_draw_sprite(gfx_headers, bitmap_dat, 4, real_height, real_width);
        case 2: return gfx_draw_gex_bitmap_16bpp(gfx_headers, bitmap_dat, real_height, real_width);
        case 1: return gfx_draw_gex_bitmap(gfx_headers, bitmap_dat, 8, real_height, real_width);
        case 0: return gfx_draw_gex_bitmap(gfx_headers, bitmap_dat, 4, real_height, real_width);
    }
    return empty_ret;
}

size_t gfx_fread_headers(FILE *gfx_headers_fp, void *dest, size_t dest_size)
{
    struct gex_gfxheader header = {0};
    struct gex_gfxchunk chunk = {0};
    size_t header_size = 28;
    u32 opmap_size = 0;
    long fpos_checkpoint = ftell(gfx_headers_fp);

    //gfxHeader parse
    if (!gex_gfxheader_parsef(gfx_headers_fp, &header))
        return 0;
    if (header._struct_pad)
        return 0;

    //gfxChunks parse
    if (!gex_gfxchunk_parsef(gfx_headers_fp, &chunk))
        return 0;
    if (chunk.height == 0 || chunk.width == 0)
        return 0;
    while (chunk.height) {
        if (!gex_gfxchunk_parsef(gfx_headers_fp, &chunk))
            return 0;
        header_size += 8;
    }

    if (header_size > 28 + IMG_CHUNKS_LIMIT * 8)
        return 0;

    if (header.type_signature & 4) {
        if (!fread_LE_U32(&opmap_size, 1, gfx_headers_fp))
            return 0;
        header_size += opmap_size;
    }

    if (opmap_size > 0xFFFF /*?*/)
        return 0;

    // file position restore and read all into the headersBuffer
    fseek(gfx_headers_fp, fpos_checkpoint, SEEK_SET);
    if (!fread(dest, 1, header_size, gfx_headers_fp)) {
        memset(dest, 0, header_size);
        return 0;
    }

    // successful end of function
    return header_size;
}

/* Obsolete? */
gfx_graphic gfx_draw_img_from_rawf(FILE *gfx_header_fp, const uint8_t *bitmap_dat)
{
    void *headers_arr = NULL;
    size_t headers_size = 0;
    u8 *new_bmp_size = NULL;
    struct gfx_graphic output = {0};

    if (!(headers_size = gfx_fread_headers(gfx_header_fp, &headers_arr, 0))) {
        return output;
    }
    if (bitmap_dat == NULL) {
        size_t bmpsize = gfx_calc_size_of_bitmap(headers_arr);

        if (!bmpsize) {
            free(headers_arr);
            return output;
        }

        new_bmp_size = malloc(bmpsize);
        if (new_bmp_size == NULL) {
            fprintf(stderr, "Err: failed to allocate memory (gfx.c::gfx_drawImgFromRawf)\n");
            exit(0xBEEF);
        }
        if (fread(new_bmp_size, 1, 1, gfx_header_fp) != bmpsize) {
            free(headers_arr);
            free(new_bmp_size);
            return output;
        }
    }
    output = gfx_draw_img_from_raw(gfx_header_fp, (bitmap_dat ? bitmap_dat : new_bmp_size));

    free(headers_arr);
    if (bitmap_dat == NULL)
        free(new_bmp_size);

    return output;
}

gfx_graphic gfx_draw_gex_bitmap(const void *chunk_headers, const u8 bitmap_dat[], uint8_t bpp, u32 min_width, u32 min_height)
{
    u8 **image = NULL;
    u32 width = 0;
    u32 height = 0;
    u16 chunk_i = 0;
    struct gex_gfxchunk chunk = {0};
    struct gfx_graphic output = {0};

    if (min_width > IMG_MAX_WIDTH || min_height > IMG_MAX_HEIGHT) {
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawGexBitmap)\n");
        return output;
    }

    if (gfx_calc_real_width_and_height(&width, &height, chunk_headers)) {
        return output;
    }

    if (width < min_width)
        width = min_width;
    if (height < min_height)
        height = min_height;

    // malloc image with valid size
    image = (u8 **) calloc2D(height, width, sizeof(u8));
    if (image == NULL) {
        fprintf(stderr, "Out Of Memory!\n");
        return output;
    }

    chunk = gex_gfxchunk_parse_aob(chunk_headers);
    uint bitmap_offset = chunk.start_offset; // This works different from the game engine

    //foreach chunk
    while (chunk.start_offset > 0) {
        const u8 *datap = bitmap_dat + chunk.start_offset - bitmap_offset;

        if (chunk.start_offset - bitmap_offset > (width * height) / (8 / bpp)) {
            //Invalid graphic / misrecognized data
            free(image);
            return output;
        }

        // Proccess Data
        for (u16 i = 0; i < chunk.height * chunk.width; i++) {
            u16 y = chunk.rel_position_y + (i / chunk.width);
            u16 x = chunk.rel_position_x + (i % chunk.width);

            image[y][x] = datap[i];
            p_chunk_rel_draw_pixel(image, &chunk, i, datap[i / (8 / bpp)], bpp);
        }

        chunk = gex_gfxchunk_parse_aob(chunk_headers + (++chunk_i * 8));
    }

    // assign output and return
    output.bitmap = image;
    output.width = width;
    output.height = height;

    return output;
}

gfx_graphic gfx_draw_gex_bitmap_16bpp(const void *chunk_headers,
                                      const void *bitmap_dat,
                                      uint32_t min_width,
                                      uint32_t min_height)
{
    void **image = NULL;
    u32 width = 0, height = 0;
    struct gex_gfxchunk chunk = {0};
    struct gfx_graphic output = {0};

    if (!chunk_headers || !bitmap_dat)
        return output;
    if (min_width > IMG_MAX_WIDTH || min_height > IMG_MAX_HEIGHT) {
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawSprite)\n");
        return output;
    }

    if (gfx_calc_real_width_and_height(&width, &height, chunk_headers)) {
        return output;
    }

    image = calloc2D(MAX(height, min_height), MAX(width, min_width), 4);
    if (!image)
        return output;

    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            u16 rgb555val = aob_read_LE_U16(bitmap_dat + (y * width + x) * 2);
            png_color color = bgr555_to_rgb888(rgb555val);
            ((u8 **) image)[y][x * 4 + 0] = color.red;
            ((u8 **) image)[y][x * 4 + 1] = color.green;
            ((u8 **) image)[y][x * 4 + 2] = color.blue;
            ((u8 **) image)[y][x * 4 + 3] = rgb555val ? 0xFF : 0;
        }
    }
    // assign output and return
    output.bitmap = (u8**)image;
    output.width = width;
    output.height = height;

    return output;
}

// TODO CONSIDER GFX HEADER ARG INSTEAD OF chunksAndOpMap AND bpp
gfx_graphic gfx_draw_sprite(const void *chunk_headers_and_opmap,
                            const u8 *bitmap_dat,
                            u8 bpp,
                            u32 min_width,
                            u32 min_height)
{
    u8 **image = NULL;
    u32 width = 0, height = 0;
    const u8 *opmapp = NULL;
    u32 opmap_size = 0;
    uint pix_ind = 0, pix_in_ch_ind = 0;
    uint chunk_ind = 0;
    struct gex_gfxchunk chunk = {0};
    struct gfx_graphic output = {0};

    if (min_width > IMG_MAX_WIDTH || min_height > IMG_MAX_HEIGHT) {
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawGexBitmap)\n");
        return output;
    }

    if (gfx_calc_real_width_and_height(&width, &height, chunk_headers_and_opmap)) {
        return output;
    }

    width = MAX(width, min_width);
    height = MAX(height, min_height);

    // malloc image with valid size
    image = (u8 **) calloc2D(height, width, sizeof(u8));
    if (image == NULL) {
        fprintf(stderr, "Out Of Memory!\n");
        return output;
    }

    // opmap size and pointer
    {
        struct gex_gfxchunk firstChunk = gex_gfxchunk_parse_aob(chunk_headers_and_opmap);
        chunk = firstChunk;
        while (chunk.start_offset) {
            chunk_ind++;
            chunk = gex_gfxchunk_parse_aob(chunk_headers_and_opmap + chunk_ind * 8);
        }
        opmapp = chunk_headers_and_opmap + chunk_ind * 8 + 12;
        opmap_size = aob_read_LE_U32(opmapp - 4) - 4;

        chunk = firstChunk;
        chunk_ind = 0;
    }

    uint bitmap_offset = chunk.start_offset; // This works different from the game engine

    for (size_t m = 0; m < opmap_size; m++) {
        uint optype = 0; // 0 - just draw pixels from bitmap, 1 - take 4 bytes and draw them over and over
        uint repeats = 0;
        u8 opval = opmapp[m];
        u8 pixVal = 0;

        if (opmapp[m] >= 0x80) {
            optype = 1;
            opval -= 0x80;
        }
        repeats = (opval ? opval * 32 : 4096) / bpp;
        for (uint i = 0; i < repeats; i++) {
            // next graphic chunk
            if (pix_in_ch_ind >= chunk.height * chunk.width) {
                chunk = gex_gfxchunk_parse_aob(chunk_headers_and_opmap + (++chunk_ind) * 8);
                if (chunk.height * chunk.width == 0)
                    return output;
                pix_in_ch_ind = 0;
            }
            switch (optype) {
                case 0: pixVal = bitmap_dat[pix_ind++ / (8 / bpp)];
                    break;
                case 1: pixVal = bitmap_dat[(pix_ind + (i % 32 / bpp)) / (8 / bpp)];
                    break;
            }
            p_chunk_rel_draw_pixel(image, &chunk, pix_in_ch_ind, pixVal, bpp);
            pix_in_ch_ind++;
        }
        if (optype == 1)
            pix_ind += 32 / bpp;
    }

    // assign output and return
    output.bitmap = image;
    output.width = width;
    output.height = height;

    return output;
}

png_color bgr555_to_rgb888(u16 bgr555)
{
    //1BBBBBGGGGGRRRRR
    png_color rgb888;
    rgb888.red = 8 * ((bgr555) & 0b11111);
    rgb888.green = 8 * ((bgr555 >> 5) & 0b11111);
    rgb888.blue = 8 * ((bgr555 >> 10) & 0b11111);

    return rgb888;
}

uint8_t gex_gfxheader_type_get_bpp(uint32_t typeSignature)
{
    switch (typeSignature & 3) {
        case 3: //?
        case 2: return 16;
        case 1: return 8;
        case 0: return 4;
    }
    return 0;
}

bool gfx_calc_real_width_and_height(u32 *ref_width, u32 *ref_height, const void *first_chunk)
{
    struct gex_gfxchunk chunk = {0};
    u8 chunk_i = 0;

    chunk = gex_gfxchunk_parse_aob((u8 *) first_chunk);
    while (chunk.start_offset > 0) {
        if (chunk_i > IMG_CHUNKS_LIMIT) {
            dbg_errlog("Error: Chunks limit reached (gfx.c::gfx_calc_real_width_and_height)\n");
            return true;
        }
        // Compare min required size with current canvas borders
        if (chunk.rel_position_x + chunk.width > *ref_width) {
            *ref_width = chunk.rel_position_x + chunk.width;
        }
        if (chunk.rel_position_y + chunk.height > *ref_height) {
            *ref_height = chunk.rel_position_y + chunk.height;
        }
        // chunk validation
        if (chunk.start_offset < 20) {
            // invalid graphic format / misrecognized data
            return true;
        }
        first_chunk += 8;
        chunk = gex_gfxchunk_parse_aob((u8 *) first_chunk);
        chunk_i++;
    }
    // canvas size validation
    if (*ref_width < 1 || *ref_height < 1 || *ref_width > IMG_MAX_WIDTH || *ref_height > IMG_MAX_HEIGHT) {
        // invalid graphic format / misrecognized data
        return true;
    }
    return false;
}

void gfx_graphic_close(gfx_graphic *g)
{
    if (g->palette)
        free(g->palette);

    if (g->bitmap)
        free(g->bitmap);
}

// TODO: Check if this works correctly
void *gfx_combine_graphic_and_bitmaps_w_alloc(const void *gfx, const void **bmps, size_t bmp_n)
{
    u8 bpp = gex_gfxheader_type_get_bpp(aob_read_LE_U32((const u8 *) gfx + 16));
    size_t total = gfx_calc_size_of_bitmap(gfx) + 20 + 8 * bmp_n + 8;
    if (total <= 28) return NULL;

    void *new_gfx = calloc(1, total);
    if (!new_gfx) return NULL;

    // header copy
    memcpy(new_gfx, gfx, 20 + 8 * bmp_n + 8);

    // signature change
    u8 type = *((u8 *) new_gfx + 16);
    *((u8 *) new_gfx + 16) = (type & 0x0F) | 0x80;

    // move pointer to the end of graphic header
    gfx = (const u8 *) gfx + 20;

    // foreach graphic chunk
    u32 bitmap_start = 20 + 8 * bmp_n + 8;
    for (size_t i = 0; i < MIN(bmp_n, IMG_CHUNKS_LIMIT); i++) {
        u16 bmp_w = 0, bmp_h = 0;
        u16 cp_w = 0, cp_h = 0;
        u16 raw_w = 0;
        struct gex_gfxchunk gchunk = gex_gfxchunk_parse_aob((const u8 *) gfx + 8 * i);
        if (gchunk.width == 0 || gchunk.height == 0) break;
        // read width and height from bitmap
        raw_w = aob_read_LE_U16((const u8 *) bmps[i]);
        bmp_h = aob_read_LE_U16((const u8 *) bmps[i] + 2);
        bmp_w = (bpp == 16 ? raw_w : raw_w * 16 / bpp);
        // validation
        if (bmp_w > IMG_MAX_WIDTH || bmp_h > IMG_MAX_HEIGHT) {
            free(new_gfx);
            return NULL;
        }
        // crop width and height of bitmap to size defined in graphic chunk
        cp_w = MIN(gchunk.width, bmp_w);
        cp_h = MIN(gchunk.height, bmp_h);

        // check if crop is supported. This may be changed in the future
        if (bpp == 4 && (bmp_w - cp_w) % 2 != 0) {
            dbg_errlog("error: gfx_combine_graphic_and_bitmaps_w_alloc: unsupported crop on 4bpp bitmap.\n"
                       "difference between bitmap width and graphic chunk width must be even number\n");
            free(new_gfx);
            return NULL;
        }

        // copy bitmap to new graphic
        if (bmp_w == cp_w && bmp_h == cp_h) {
            memcpy((u8 *) new_gfx + bitmap_start, (const u8 *) bmps[i] + 4, cp_w * raw_w * 2);
        } else {
            for (u16 y = 0; y < cp_h; y++) {
                size_t n = (bpp == 16 ? cp_w * 2 : cp_w / (8 / bpp));
                memcpy((u8 *) new_gfx + bitmap_start + y * gchunk.width, (u8 *) bmps[i] + 4 + y * bmp_w, n);
            }
        }

        // change start offset in graphic chunk header
        u16 bs = aob_read_LE_U16(&bitmap_start); // byte swap on big endian platforms
        *((u16 *)((u8*)new_gfx + 20 + 8 * i)) = bs;

        bitmap_start += cp_w * raw_w;
    }

    return new_gfx;
}

size_t gfx_calc_total_size_of_graphic(const void *graphic, size_t buff_size)
{
    size_t size = 28;

    size = gfx_calc_size_of_headers(graphic, buff_size);
    if (size == 0 || size >= buff_size) return 0;

    if (*((const u8 *) graphic + 16) & 4) {
        size += gfx_calc_size_of_sprite(graphic);
    } else {
        size += gfx_calc_size_of_bitmap(graphic);
    }
    return size;
}

size_t gfx_calc_size_of_headers(const void *headers, size_t buff_size)
{
    size_t size = 28;
    const u8 *gptr = headers + 20;

    if (buff_size < 28) return buff_size;

    // gfxHeader parse
    struct gex_gfxheader gheader = gex_gfxheader_parse_aob(headers);

    // quick validation
    if (gheader._struct_pad || gheader.type_signature == 0
        || gheader.inf_img_width > IMG_MAX_WIDTH || gheader.inf_img_height > IMG_MAX_HEIGHT)
        return 0;

    // gfxChunks count
    struct gex_gfxchunk gchunk = gex_gfxchunk_parse_aob(gptr);

    for (uint i = 0; gchunk.height && gchunk.width && i < IMG_CHUNKS_LIMIT; i++) {
        size += 8;
        gptr += 8;
        if (size + 8 > buff_size) return buff_size;
        gchunk = gex_gfxchunk_parse_aob(gptr);
    }
    gptr += 8;

    if (size > 28 + IMG_CHUNKS_LIMIT * 8)
        return 0;

    // map of operations size of sprite
    if (gheader.type_signature & 4) {
        if (size + 4 > buff_size) return buff_size;
        size += aob_read_LE_U32(gptr);

        if (aob_read_LE_U32(gptr) > 0xFFFF /*?*/)
            return 0;
    }

    return size;
}


// -----------------------------------------------------
// STATIC FUNCTION DEFINITIONS:
// -----------------------------------------------------

// draw pixel relative of chunk position
static void p_chunk_rel_draw_pixel(u8 **img, const struct gex_gfxchunk *chunk, u16 cpix_i, u8 pix_val, u8 bpp)
{
    u16 y = chunk->rel_position_y + (cpix_i / chunk->width);
    u16 x = chunk->rel_position_x + (cpix_i % chunk->width);

    if (bpp == 8)
        img[y][x] = pix_val;
    else {
        u8 shift = bpp * (cpix_i % (8 / bpp));
        u8 mask = 0xFF >> (8 - bpp);
        img[y][x] = (pix_val >> shift) & mask;
    }
}

static void **malloc2D(u32 y, u32 x, u8 element_size)
{
    void **arr = (void **) malloc(sizeof(uintptr_t) * y + element_size * x * y);
    uintptr_t addr = (uintptr_t) &arr[y];

    for (u32 i = 0; i < y; i++)
        arr[i] = (void *) (addr + i * element_size * x);

    return arr;
}

static void **calloc2D(u32 y, u32 x, u8 element_size)
{
    void **arr = (void **) calloc(sizeof(uintptr_t) * y + element_size * x * y, 1);
    uintptr_t addr = (uintptr_t) &arr[y];

    for (u32 i = 0; i < y; i++)
        arr[i] = (void *) (addr + i * element_size * x);

    return arr;
}

