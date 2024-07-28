#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "filescanning.h"
#include "common.h"
#include "../helpers/binary_parse.h"

// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr p_gexptr_to_offset(u32 gexptr, uptr start_offset);

static u32 p_offset_to_gexptr(uptr offset, uptr file_start_offset);

// part of fscan_init
static int
p_files_init_open_and_set(const char filename[], FILE *general_fp, size_t fsize, fscan_file_chunk fchunk[1]);

static void p_close_fchunk(fscan_file_chunk *fchp);

static int p_read_and_draw_single_graphic(fscan_file_chunk *bmp_fchp,
                                          fscan_file_chunk *gfx_fchp,
                                          const fscan_gfx_info *ginf,
                                          gfx_graphic *output);

static void calc_output_dimensions(const fscan_gfx_info ginf[],
                                   size_t ginf_n,
                                   uint out_width[1],
                                   uint out_height[1],
                                   int out_origin_x[1],
                                   int out_origin_y[1]);

// _______________________________________________________ FUNCTION DEFINITIONS _______________________________________________________

/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static inline int
p_files_init_open_and_set(const char filename[], FILE *general_fp, size_t fsize, fscan_file_chunk fchunk[1])
{
    fread_LE_U32((u32 *) &fchunk->size, 1, general_fp);
    fread_LE_U32(&fchunk->offset, 1, general_fp);
    if (!(fchunk->offset && fchunk->size > 32 && fchunk->offset + fchunk->size <= fsize))
        return 1;
    if (!(fchunk->fp = fopen(filename, "rb")) || !(fchunk->data_fp = fopen(filename, "rb"))) {
        return -1;
    }
    // non-ptr int arithmetics below.  Setting initial stream positions & Entry point offset
    u32 epOffset = fchunk->offset + fchunk->size / 2048 + 4; //< entry point address for ptrs lookup
    fseek(fchunk->data_fp, fchunk->offset, SEEK_SET);
    fseek(fchunk->fp, epOffset, SEEK_SET);
    fread_LE_U32(&fchunk->ep, 1, fchunk->fp);
    fchunk->ep = (u32) p_gexptr_to_offset(fchunk->ep, fchunk->offset);

    return 0;
}

static inline void *
p_read_ext_bmp_and_header_then_combine(fscan_file_chunk *bmp_fchp,
                                       fscan_file_chunk *header_fchp,
                                       const fscan_gfx_info *ginf);

int fscan_files_init(fscan_files *sf, const char filename[])
{
    FILE *fp = NULL;
    u32 fchunkcnt = 0;
    size_t fsize = 0;

    // zeroing members
    sf->ext_bmp_counter = 0;

    fp = fopen(filename, "rb");
    if (fp == NULL)
        return FSCAN_LEVEL_TYPE_FOPEN_ERROR;

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);

    if (fsize < FILE_MIN_SIZE)
        return FSCAN_LEVEL_TYPE_FILE_TOO_SMALL;
    //read first value
    rewind(fp);
    if (!fread_LE_U32(&fchunkcnt, 1, fp))
        return FSCAN_READ_ERROR_FREAD;

    // Check file type
    if (fchunkcnt >= 5 && fchunkcnt <= 32) {
        //FILE TYPE: STANDARD LEVEL

        fseek(fp, 0x18, SEEK_SET);
        for (size_t i = 0; i < FILE_CHUNKS; i++) {
            if (p_files_init_open_and_set(filename, fp, fsize, &sf->file_chunks[i]) == -1)
                return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            fseek(fp, 8, SEEK_CUR);
        }

        if (fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS))
            if (gexdev_u32vec_init_capcity(&sf->ext_bmp_offsets, 256))
                exit(ERR_OUT_OF_MEMORY);

    } else {
        // FILE TYPE: standalone gfx file
        // TODO: more special files detection
    }

    fclose(fp);
    return 0;
}

void fscan_files_close(fscan_files *files_stp)
{
    for (size_t i = 0; i < FILE_CHUNKS; i++) {
        p_close_fchunk(&files_stp->file_chunks[i]);
    }

    for (int i = 0; i < TILE_BMP_MAX_CHUNKS; i++)
        gexdev_u32vec_close(&files_stp->tile_ext_bmp_offsets[i]);
    gexdev_u32vec_close(&files_stp->ext_bmp_offsets);
}

u32 fscan_read_gexptr(FILE *fp, uint32_t chunk_offset, jmp_buf *error_jmp_buf)
{
    u32 val = 0;
    if (!fread_LE_U32(&val, 1, fp) && error_jmp_buf)
        longjmp(*error_jmp_buf, FSCAN_READ_ERROR_FREAD);

    return (u32) p_gexptr_to_offset(val, chunk_offset);
}

size_t fscan_fread(void *dest, size_t size, size_t n, FILE *fp, jmp_buf *error_jmp_buf)
{
    size_t retval = fread(dest, size, n, fp);
    if (retval < n && error_jmp_buf)
        longjmp(*error_jmp_buf, FSCAN_READ_ERROR_FREAD);
    return retval;
}

size_t
fscan_read_header_and_bitmaps_alloc(fscan_file_chunk *fchp, fscan_file_chunk *extbmpchunkp, void **header_and_bitmapp,
                                    void **bmp_startpp, const u32 ext_bmp_offsets[], size_t ext_bmp_offsets_size,
                                    unsigned int *bmp_indexp, jmp_buf(*errbufp), gexdev_paged_map *header_bmp_bindsp)
{
    size_t header_size = 0;
    size_t total_bmp_size = 0;
    struct gex_gfxheader gfxheader = {0};
    bool is_bmp_extern = false;
    u32 header_offset = fscan_read_gexptr(fchp->fp, fchp->offset, errbufp);

    // header read
    fseek(fchp->data_fp, header_offset, SEEK_SET);
    gex_gfxheader_parsef(fchp->data_fp, &gfxheader);

    if ((gfxheader.type_signature & 0xF0) == 0xC0) {
        is_bmp_extern = true;
        if (extbmpchunkp->fp == NULL) {
            fprintf(stderr,
                    "error: fscan_read_header_and_bitmaps_alloc extbmpchunkp param does not point a valid file chunk\n");
            return 0;
        }
    }

    fseek(fchp->data_fp, header_offset, SEEK_SET);

    header_size = gfx_fread_headers(fchp->data_fp, header_and_bitmapp, 0);
    if (!header_size)
        return 0;

    total_bmp_size = gfxheader.type_signature & 4 ? gfx_calc_size_of_sprite(*header_and_bitmapp) :
                     gfx_calc_size_of_bitmap(*header_and_bitmapp);

    if (!total_bmp_size) {
        free(*header_and_bitmapp);
        return 0;
    }
    if (!(*header_and_bitmapp = realloc(*header_and_bitmapp, header_size + total_bmp_size)))
        exit(0xA4C3D);

    *bmp_startpp = *header_and_bitmapp + header_size;

    if (is_bmp_extern) {
        // bitmap in bitmap file chunk
        u32 rel_header_off = header_offset - fchp->offset;
        u8 *bmp_from_map = NULL;

        if ((bmp_from_map = gexdev_paged_map_get(header_bmp_bindsp, &rel_header_off))) {
        } else {
            size_t written_bmp_bytes = 0;
            if (!(bmp_from_map = malloc(total_bmp_size + header_size)))
                exit(0xB4C3D); // freed in gexdev_ptr_map_close_all

            for (void *gchunk = *header_and_bitmapp + 20; *(u32 *) gchunk; gchunk += 8) {
                size_t bmp_part_size = 0;
                u16 gchunk_off = written_bmp_bytes + 36;
                u16 sizes[2] = {0};

                if (ext_bmp_offsets_size <= *bmp_indexp)
                    longjmp(*errbufp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

                // bitmap sizes check
                fseek(extbmpchunkp->data_fp, ext_bmp_offsets[*bmp_indexp], SEEK_SET);
                if (fread_LE_U16(sizes, 2, extbmpchunkp->data_fp) != 2)
                    longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

                bmp_part_size = sizes[0] * sizes[1] * 2;

                if (written_bmp_bytes + bmp_part_size > total_bmp_size)
                    longjmp(*errbufp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

                // read bitmap
                if (fread(bmp_from_map + header_size + written_bmp_bytes, 1, bmp_part_size, extbmpchunkp->data_fp) <
                    bmp_part_size)
                    longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

                // overwrite chunk data start offset
                aob_read_LE_U16(&gchunk_off);
                *(u16 *) gchunk = gchunk_off;

                written_bmp_bytes += bmp_part_size;
                (*bmp_indexp)++;
            }
            //! DEBUG. NOTE: MAY BE WRONG
            if (written_bmp_bytes < total_bmp_size) {
                printf("DEBUG INFO: read bitmap bytes and expected bitmap size difference: %lu\n",
                       total_bmp_size - written_bmp_bytes);
            }
            memcpy(bmp_from_map, *header_and_bitmapp, header_size); // copy header before mapping
            gexdev_paged_map_set(header_bmp_bindsp, &rel_header_off, bmp_from_map);
        }
        memcpy(*header_and_bitmapp, bmp_from_map, total_bmp_size + header_size);
    } else {
        // bitmap next to the header
        if (fread(*bmp_startpp, 1, total_bmp_size, fchp->data_fp) < total_bmp_size)
            longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);
    }

    return header_size + total_bmp_size;
}

uint32_t fscan_read_gexptr_and_follow(fscan_file_chunk *fchp, int addoff, jmp_buf(*errbufp))
{
    u32 gexptr = fscan_read_gexptr(fchp->fp, fchp->offset, errbufp);
    if (!gexptr || gexptr >= fchp->offset + fchp->size)
        return 0;
    fseek(fchp->fp, gexptr + addoff, SEEK_SET);
    return gexptr;
}

size_t fscan_read_gexptr_null_term_arr(fscan_file_chunk *fchp, uint32_t dest[], size_t dest_size, jmp_buf(*errbufp))
{
    for (uint i = 0; i < dest_size - 1; i++)
        if (!(dest[i] = fscan_read_gexptr(fchp->fp, fchp->offset, errbufp)) ||
            dest[i] >= fchp->size + fchp->offset - 4) {
            dest[i] = 0;
            return i;
        }
    dest[dest_size - 2] = 0;
    return dest_size - 1;
}

inline static void p_read_arr_of_bmp_ptrs_and_push_valid_bmp_offs_to_vec(fscan_file_chunk *sf,
                                                                         gexdev_u32vec *vecp,
                                                                         jmp_buf(*errbufp))
{
    u32 bmp_offsets[256] = {0};
    fscan_read_gexptr_null_term_arr(sf, bmp_offsets, 256, errbufp);
    for (int ii = 0; ii < 256 && bmp_offsets[ii]; ii++) {
        u16 wh[2] = {0};
        fseek(sf->data_fp, bmp_offsets[ii], SEEK_SET);
        if (fread_LE_U16(wh, 2, sf->data_fp) != 2)
            longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

        if (wh[0] && wh[1])
            gexdev_u32vec_push_back(vecp, bmp_offsets[ii]);
    }
}

const gexdev_u32vec *fscan_search_for_ext_bmps(fscan_files *sf)
{
    u32 block_offsets[6] = {0};
    jmp_buf *errbufp = sf->error_jmp_buf;
    fscan_file_chunk *bmpc = &sf->file_chunks[FCH_TYPE_EXT_BITMAPS];
    gexdev_u32vec *ebmp_offs = &sf->ext_bmp_offsets;

    if (ebmp_offs->size || !bmpc->fp)
        return ebmp_offs;

    fseek(bmpc->fp, bmpc->ep, SEEK_SET);

    for (int i = 0; i < 6; i++) {
        block_offsets[i] = fscan_read_gexptr(bmpc->fp, bmpc->offset, errbufp);
    }

    for (int i = 0; i < 6; i++) {
        if (block_offsets[i]
            && block_offsets[i] <= bmpc->size + bmpc->offset - 4)  // ???
        {
            fseek(bmpc->fp, block_offsets[i], SEEK_SET);
            p_read_arr_of_bmp_ptrs_and_push_valid_bmp_offs_to_vec(bmpc, ebmp_offs, errbufp);
        }
    }
    return ebmp_offs;
}

int fscan_search_for_tile_bmps(fscan_files *sf)
{
    u32 block_offsets[TILE_BMP_MAX_CHUNKS] = {0};
    jmp_buf *errbufp = sf->error_jmp_buf;
    gexdev_u32vec *vecs = sf->tile_ext_bmp_offsets;
    fscan_file_chunk *tile_bmp_ch = &sf->file_chunks[FCH_TYPE_TILE_BITMAPS];

    if (!tile_bmp_ch->fp)
        return 1;

    fseek(tile_bmp_ch->fp, tile_bmp_ch->ep, SEEK_SET);
    fscan_read_gexptr_null_term_arr(tile_bmp_ch, block_offsets, sizeofarr(block_offsets), errbufp);

    for (int i = 0; i < sizeofarr(block_offsets) && block_offsets[i]; i++) {
        fseek(tile_bmp_ch->fp, block_offsets[i], SEEK_SET);
        if (vecs[i].v) gexdev_u32vec_close(&vecs[i]);
        gexdev_u32vec_init_capcity(&vecs[i], 64);
        p_read_arr_of_bmp_ptrs_and_push_valid_bmp_offs_to_vec(tile_bmp_ch,
                                                              &vecs[i],
                                                              errbufp);
    }
    return 0;
}

void fscan_gfx_info_close(fscan_gfx_info *ginf)
{
    if (!ginf) return;
    if (ginf->ext_bmp_offsets) {
        ginf->ext_bmp_offsets = NULL;
    }
}

void fscan_scan_result_close(fscan_gfx_info_vec *result)
{
    if (!result || !result->base.v) return;
    for (size_t i = 0; i < result->base.size; i++) {
        fscan_gfx_info_close(fscan_gfx_info_vec_at(result, i));
    }
    gexdev_univec_close(&result->base);
}

void fscan_gfx_info_vec_close(fscan_gfx_info_vec *vecp)
{
    fscan_scan_result_close(vecp);
}

fscan_gfx_info *fscan_gfx_info_vec_at(const fscan_gfx_info_vec *vecp, size_t index)
{
    if (vecp->base.size <= index)
        return NULL;
    return &((fscan_gfx_info *) vecp->base.v)[index];
}

int fscan_draw_gfx_using_gfx_info_ex(fscan_files *sf,
                                     const fscan_gfx_info *ginf, size_t ginf_n,
                                     int gfx_category,
                                     int pos_x, int pos_y, int flags,
                                     gfx_graphic *output)
{
    gfx_graphic *graphics = calloc(ginf_n, sizeof(gfx_graphic));
    fscan_file_chunk *fchp = &sf->file_chunks[fscan_get_gfx_category_header_origin(gfx_category)];
    fscan_file_chunk *bmpchp = &sf->file_chunks[fscan_get_gfx_category_ext_bmp_origin(gfx_category)];

    for (size_t gi = 0; gi < ginf_n; gi++) {
        int errcode = p_read_and_draw_single_graphic(bmpchp, fchp, &ginf[gi], &graphics[gi]);
        if (errcode) {
            for (size_t i = 0; i <= gi; i++) {
                gfx_graphic_close(&graphics[i]);
            }
            free(graphics);
            return -1;
        }
    }

    uint w = 0, h = 0;
    int ox = 0, oy = 0;
    calc_output_dimensions(ginf, ginf_n, &w, &h, &ox, &oy);

    // check if the total size of output wouldn't be too big
    if (w > IMG_MAX_WIDTH || h > IMG_MAX_HEIGHT) {
        dbg_errlog_va("Image too big (%dx%d). Limit is %dx%d\n", w, h, IMG_MAX_WIDTH, IMG_MAX_HEIGHT);
        for (int i = 0; i < ginf_n; i++)
            gfx_graphic_close(&graphics[i]);
        free(graphics);
        return -2;
    }

    // draw the graphics on output canvas
    if (ginf_n > 1) {
        output->width = w;
        output->height = h;
        output->bitmap = calloc2D(h, w, graphics[0].palette ? 1 : 4);
        for (int i = 0; i < ginf_n; i++) {
            gfx_graphic_merge(output, &graphics[i], ginf[i].gfx_props.pos_x - ox, ginf[i].gfx_props.pos_y - oy);
            gfx_graphic_close(&graphics[i]);
        }
    } else {
        *output = graphics[0];
    }

    free(graphics);
    return 0;
}

int fscan_draw_gfx_using_gfx_info(fscan_files *files_stp,
                                  const fscan_gfx_info ginf[],
                                  size_t ginf_n,
                                  int src_file_chunk_ind,
                                  gfx_graphic *output)
{
    return fscan_draw_gfx_using_gfx_info_ex(files_stp, ginf, ginf_n, src_file_chunk_ind, 0, 0, 0, output);
}

bool fscan_files_is_chunk_existing(const fscan_files *sf, size_t chunk_ind)
{
    return sf->file_chunks[chunk_ind].fp ? true : false;
}

static uptr p_gexptr_to_offset(u32 gexptr, uptr start_offset)
{
    if (gexptr == 0)
        return 0;
    return start_offset + (gexptr >> 20) * 0x2000 + (gexptr & 0xFFFF) - 1;
}

static u32 p_offset_to_gexptr(uptr offset, uptr file_start_offset)
{
    offset -= file_start_offset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}

void p_close_fchunk(fscan_file_chunk *fchp)
{
    if (fchp->fp) {
        fclose(fchp->fp);
        fchp->fp = NULL;
    }
    if (fchp->data_fp) {
        fclose(fchp->data_fp);
        fchp->data_fp = NULL;
    }
}

void *p_read_ext_bmp_and_header_then_combine(fscan_file_chunk *bmp_fchp,
                                             fscan_file_chunk *header_fchp,
                                             const fscan_gfx_info *ginf)
{
    void *raw_graphic = NULL;
    void *gheader = NULL;
    void *bitmaps[IMG_CHUNKS_LIMIT] = {0};


    // read all bitmaps
    for (int i = 0; i < ginf->chunk_count; i++) {
        u32 bmp_offset = ginf->ext_bmp_offsets[i];
        u16 wh[2] = {0};
        if (!bmp_offset) {
            dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: invalid bmp offset (Should not happen)\n");
            return NULL;
        }
        // read size of bitmap
        fseek(bmp_fchp->fp, bmp_offset, SEEK_SET);
        fread_LE_U16(wh, 2, bmp_fchp->fp);

        if (*(u32 *) wh == 0) {
            dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: file read error\n");
            return NULL;
        }

        if (wh[0] / 4 > IMG_MAX_WIDTH || wh[1] > IMG_MAX_HEIGHT) {
            dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: bitmap size out of limits\n");
            return NULL;
        }

        // malloc bitmap in bitmaps array
        bitmaps[i] = malloc(wh[0] * wh[1] * 2);
        if (!bitmaps[i]) {
            dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: malloc error\n");
            exit(ERR_OUT_OF_MEMORY);
        }

        // rewind to the start of the bitmap with the size
        fseek(bmp_fchp->fp, -4, SEEK_CUR);

        // read bitmap
        if (fread(bitmaps[i], 2, wh[0] * wh[1] + 2, bmp_fchp->fp) != wh[0] * wh[1] + 2) {
            dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: file read error\n");
            for (int ii = 0; ii <= i; ii++) {
                free(bitmaps[ii]);
            }
            return NULL;
        }
    }
    // allocate memory for graphic header
    gheader = malloc(28 + 8 * ginf->chunk_count);
    if (!gheader) {
        dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: malloc error\n");
        exit(ERR_OUT_OF_MEMORY);
    }

    // read graphic header
    fseek(header_fchp->fp, ginf->gfx_offset, SEEK_SET);
    if (fread(gheader, 1, 28 + 8 * ginf->chunk_count, header_fchp->fp) != 28 + 8 * ginf->chunk_count) {
        dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: file read error\n");
        free(gheader);
        for (int i = 0; i < IMG_CHUNKS_LIMIT && bitmaps[i]; i++)
            free(bitmaps[i]);
        return NULL;
    }
    // combine header and bitmaps
    raw_graphic = gfx_combine_graphic_and_bitmaps_w_alloc(gheader, (const void **) bitmaps, ginf->chunk_count);

    // cleanup
    free(gheader);
    for (int i = 0; i < IMG_CHUNKS_LIMIT && bitmaps[i]; i++) {
        free(bitmaps[i]);
    }

    return raw_graphic;
}

int p_read_and_draw_single_graphic(fscan_file_chunk *bmp_fchp,
                                   fscan_file_chunk *gfx_fchp,
                                   const fscan_gfx_info *ginf,
                                   gfx_graphic *output)
{
    void *raw_graphic = NULL;
    uint IDAT_off = 0;

    // Argument check
    if (ginf->gfx_offset == 0) {
        return -1;
    }

    if (ginf->ext_bmp_offsets) {
        raw_graphic = p_read_ext_bmp_and_header_then_combine(bmp_fchp, gfx_fchp, ginf);

        if (!raw_graphic) return -2;
        IDAT_off = gfx_calc_size_of_headers(raw_graphic, 99999 /* change to be more memory safety? */);
    } else {
        // Graphic header parse
        struct gex_gfxheader gheader = {0};
        fseek(gfx_fchp->fp, ginf->gfx_offset, SEEK_SET);
        gex_gfxheader_parsef(gfx_fchp->fp, &gheader);
        fseek(gfx_fchp->fp, -20, SEEK_CUR);

        if ((gheader.type_signature & 0xF0) == 0xC0) {
            dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: file chunk with bitmaps is missing"
                       " or fscan_gfx_info object is corrupted.\n");
            return -3;
        } else {
            // First we need to find out the size of the graphic
            u8 headers[2048 * 8];
            size_t IDAT_size = 0;
            size_t size = IDAT_off = gfx_fread_headers(gfx_fchp->fp, &headers, sizeof(headers));
            if (!size) return -4;

            if (gheader.type_signature & 4) {
                IDAT_size = gfx_calc_size_of_sprite(headers);
            } else {
                IDAT_size = gfx_calc_size_of_bitmap(headers);
            }

            if (!IDAT_size) return -5;

            size += IDAT_size;

            // Allocate memory for the graphic
            raw_graphic = malloc(size);
            if (!raw_graphic) {
                dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: malloc error\n");
                exit(ERR_OUT_OF_MEMORY);
            }

            // Read the graphic
            fseek(gfx_fchp->fp, ginf->gfx_offset, SEEK_SET);
            if (fread(raw_graphic, size, 1, gfx_fchp->fp) != 1) {
                dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: graphic read error\n");
                free(raw_graphic);
                return -6;
            }
        }
    }

    // Draw the graphic
    *output = gfx_draw_img_from_raw(raw_graphic, raw_graphic + IDAT_off);

    // TODO: Cache palettes
    // TODO: Verify palette
    // Read and parse color palette
    if (ginf->palette_offset) {
        output->palette = malloc(sizeof(gfx_palette));
        fseek(gfx_fchp->fp, ginf->palette_offset, SEEK_SET);
        if (!gfx_palette_parsef(gfx_fchp->fp, output->palette)) {
            dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: palette read error\n");
            gfx_graphic_close(output);
            free(raw_graphic);
            return -7;
        }
    }

    // Free the raw graphic
    free(raw_graphic);

    if (!output->bitmap) {
        dbg_errlog("error: fscan_draw_gfx_using_gfx_info_ex: graphic draw error\n");
        return -8;
    }

    return 0;
}

void calc_output_dimensions(const fscan_gfx_info ginf[],
                            size_t ginf_n,
                            uint out_width[1],
                            uint out_height[1],
                            int out_origin_x[1],
                            int out_origin_y[1])
{
    int min_x = INT_MAX;
    int min_y = INT_MAX;
    int max_x = INT_MIN;
    int max_y = INT_MIN;

    if (ginf_n == 1) {
        *out_width = ginf->width;
        *out_height = ginf->height;
    }

    for (size_t i = 0; i < ginf_n; i++) {
        min_y = MIN(ginf[i].gfx_props.pos_y, min_y);
        min_x = MIN(ginf[i].gfx_props.pos_x, min_x);
        max_y = MAX(ginf[i].gfx_props.pos_y + ginf[i].height, max_y);
        max_x = MAX(ginf[i].gfx_props.pos_x + ginf[i].width, max_x);
    }

    *out_width = max_x - min_x;
    *out_height = max_y - min_y;
    *out_origin_x = min_x;
    *out_origin_y = min_y;
}

fscan_gfx_info_vec fscan_gfx_info_vec_create(int gfx_category)
{
    fscan_gfx_info_vec result = {.gfx_category = gfx_category};
    gexdev_univec_init_capcity(&result.base, 128, sizeof(fscan_gfx_info));
    return result;
}

int fscan_gfx_scan(struct fscan_files *sf, fscan_gfx_info_vec *res_vec, int gfx_category)
{
    if (!res_vec->base.v) {
        fscan_gfx_info_vec newv = fscan_gfx_info_vec_create(gfx_category);
        *res_vec = newv;
    } else if (res_vec->gfx_category != gfx_category) {
        dbg_errlog("Warning: gfx_category of vector and scan argument mismatch");
    }

    if (gfx_category == GFX_CAT_TILE)
        return fscan_tiles_scan(sf, res_vec);
    if (gfx_category == GFX_CAT_OBJ)
        return fscan_obj_gfx_scan(sf, res_vec);
    if (gfx_category == GFX_CAT_INTRO_OBJ)
        return fscan_intro_obj_gfx_scan(sf, res_vec);
    if (gfx_category == GFX_CAT_BACKGROUND)
        return fscan_background_scan(sf, res_vec);
    return -20;
}

int fscan_get_gfx_category_header_origin(int gfx_category)
{
    switch (gfx_category) {
        case GFX_CAT_TILE:
        case GFX_CAT_OBJ: return FCH_TYPE_MAIN;
        case GFX_CAT_INTRO_OBJ: return FCH_TYPE_INTRO;
        case GFX_CAT_BACKGROUND: return FCH_TYPE_BACKGROUND;
        default:dbg_errlog("error: unrecognized graphic category!");
            return -1;
    }
}

int fscan_get_gfx_category_ext_bmp_origin(int gfx_category)
{
    switch (gfx_category) {
        case GFX_CAT_TILE: return FCH_TYPE_TILE_BITMAPS;
        case GFX_CAT_OBJ:
        case GFX_CAT_INTRO_OBJ:
        case GFX_CAT_BACKGROUND: return FCH_TYPE_EXT_BITMAPS;
        default:dbg_errlog("error: unrecognized graphic category!");
            return -1;
    }
}

void fscan_gfx_process_results(fscan_gfx_info_vec *results,
                               u32 group_mask,
                               enum fscan_gfx_result_action action,
                               void *output)
{

}
