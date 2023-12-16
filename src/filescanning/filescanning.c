#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "filescanning.h"
#include "../helpers/binary_parse.h"

// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr p_gexptr_to_offset(u32 gexptr, uptr start_offset);

static u32 p_offset_to_gexptr(uptr offset, uptr file_start_offset);

// part of fscan_init
static inline int
p_files_init_open_and_set(const char filename[], FILE *general_fp, size_t fsize, fscan_file_chunk fchunk[1]);

inline static void p_close_fchunk(fscan_file_chunk *fchp);

// _______________________________________________________ FUNCTION DEFINITIONS _______________________________________________________

/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static inline int
p_files_init_open_and_set(const char filename[], FILE *general_fp, size_t fsize, fscan_file_chunk fchunk[1])
{
    fread_LE_U32((u32 *) &fchunk->size, 1, general_fp);
    fread_LE_U32(&fchunk->offset, 1, general_fp);
    if (!(fchunk->offset && fchunk->size > 32 && fchunk->offset + fchunk->size <= fsize))
        return 1;
    if (!(fchunk->ptrs_fp = fopen(filename, "rb")) || !(fchunk->data_fp = fopen(filename, "rb"))) {
        return -1;
    }
    // non-ptr int arithmetics below.  Setting initial stream positions & Entry point offset
    u32 epOffset = fchunk->offset + fchunk->size / 2048 + 4; //< entry point address for ptrs lookup
    fseek(fchunk->data_fp, fchunk->offset, SEEK_SET);
    fseek(fchunk->ptrs_fp, epOffset, SEEK_SET);
    fread_LE_U32(&fchunk->ep, 1, fchunk->ptrs_fp);
    fchunk->ep = (u32) p_gexptr_to_offset(fchunk->ep, fchunk->offset);

    return 0;
}

int fscan_files_init(fscan_files *files_stp, const char filename[])
{
    FILE *fp = NULL;
    u32 fchunkcnt = 0;
    size_t fsize = 0;
    int retval = 0;

    // zeroing members
    files_stp->ext_bmp_counter = 0;

    if (gexdev_u32vec_init_capcity(&files_stp->ext_bmp_offsets, 256))
        exit(0x1234);

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
        return -3;

    // Check file type
    if (fchunkcnt >= 5 && fchunkcnt <= 32) {
        //FILE TYPE: STANDARD LEVEL

        // Tile bitmaps chunk setup
        fseek(fp, 0x28, SEEK_SET);
        switch (p_files_init_open_and_set(filename, fp, fsize, &files_stp->tile_chunk)) {
            case -1:fclose(fp);
                return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 0:
                if (gexdev_u32vec_init_capcity(&files_stp->tile_bmp_offsets, 256))
                    exit(0x1234);
                break;
            case 1:retval |= FSCAN_LEVEL_FLAG_NO_TILES;
                break; // invalid / non-exsiting chunk
        }
        // Chunk with bitmaps (of backgrounds and objects) setup
        fseek(fp, 8, SEEK_CUR);
        switch (p_files_init_open_and_set(filename, fp, fsize, &files_stp->bitmap_chunk)) {
            case -1:fclose(fp);
                return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 1:retval |= FSCAN_LEVEL_FLAG_NO_BACKGROUND;
                break; // invalid / non-exsiting chunk
        }
        // Main chunk setup
        fseek(fp, 8, SEEK_CUR);
        switch (p_files_init_open_and_set(filename, fp, fsize, &files_stp->main_chunk)) {
            case -1:fclose(fp);
                return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 0:gexdev_univec_init_capcity(&files_stp->obj_gfx_offsets, 1024, sizeof(fscan_gfx_loc_info));
                break;
            case 1:retval |= FSCAN_LEVEL_FLAG_NO_MAIN;
                break; // invalid / non-exsiting chunk
        }
        // Intro chunk setup
        fseek(fp, 8, SEEK_CUR);
        switch (p_files_init_open_and_set(filename, fp, fsize, &files_stp->intro_chunk)) {
            case -1:fclose(fp);
                return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 0:gexdev_univec_init_capcity(&files_stp->intro_gfx_offsets, 1024, sizeof(fscan_gfx_loc_info));
                break;
            case 1:retval |= FSCAN_LEVEL_FLAG_NO_INTRO;
                break; // invalid / non-exsiting chunk
        }
        // Background chunk setup
        fseek(fp, 8, SEEK_CUR);
        switch (p_files_init_open_and_set(filename, fp, fsize, &files_stp->bg_chunk)) {
            case -1:fclose(fp);
                return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 0:gexdev_univec_init_capcity(&files_stp->bg_gfx_offsets, 1024, sizeof(fscan_gfx_loc_info));
                break;
            case 1:retval |= FSCAN_LEVEL_FLAG_NO_BACKGROUND;
                break; // invalid / non-exsiting chunk
        }
    } else {
        // FILE TYPE: standalone gfx file
        // TODO: more special files detection
        retval = 1;
    }

    if (fp != NULL)
        fclose(fp);
    return retval;
}

void fscan_files_close(fscan_files *files_stp)
{
    p_close_fchunk(&files_stp->tile_chunk);
    p_close_fchunk(&files_stp->bitmap_chunk);
    p_close_fchunk(&files_stp->main_chunk);
    p_close_fchunk(&files_stp->intro_chunk);
    p_close_fchunk(&files_stp->bg_chunk);

    gexdev_univec_close(&files_stp->tile_gfx_offsets);
    gexdev_univec_close(&files_stp->obj_gfx_offsets);
    gexdev_univec_close(&files_stp->intro_gfx_offsets);
    gexdev_univec_close(&files_stp->bg_gfx_offsets);
    gexdev_u32vec_close(&files_stp->ext_bmp_offsets);
    gexdev_u32vec_close(&files_stp->tile_bmp_offsets);
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
                                    unsigned int *bmp_indexp, jmp_buf(*errbufp), gexdev_ptr_map *header_bmp_bindsp)
{
    size_t header_size = 0;
    size_t total_bmp_size = 0;
    struct gex_gfxheader gfxheader = {0};
    bool is_bmp_extern = false;
    u32 header_offset = fscan_read_gexptr(fchp->ptrs_fp, fchp->offset, errbufp);

    // header read
    fseek(fchp->data_fp, header_offset, SEEK_SET);
    gex_gfxheader_parsef(fchp->data_fp, &gfxheader);

    if ((gfxheader.type_signature & 0xF0) == 0xC0) {
        is_bmp_extern = true;
        if (extbmpchunkp->ptrs_fp == NULL) {
            fprintf(stderr,
                    "error: fscan_read_header_and_bitmaps_alloc extbmpchunkp param does not point a valid file chunk\n");
            return 0;
        }
    }

    fseek(fchp->data_fp, header_offset, SEEK_SET);

    header_size = gfx_read_headers_alloc_aob(fchp->data_fp, header_and_bitmapp);
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

        if ((bmp_from_map = gexdev_ptr_map_get(header_bmp_bindsp, &rel_header_off))) {
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
            gexdev_ptr_map_set(header_bmp_bindsp, &rel_header_off, bmp_from_map);
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
    u32 gexptr = fscan_read_gexptr(fchp->ptrs_fp, fchp->offset, errbufp);
    if (!gexptr || gexptr >= fchp->offset + fchp->size)
        return 0;
    fseek(fchp->ptrs_fp, gexptr + addoff, SEEK_SET);
    return gexptr;
}

size_t fscan_read_gexptr_null_term_arr(fscan_file_chunk *fchp, uint32_t dest[], size_t dest_size, jmp_buf(*errbufp))
{
    for (uint i = 0; i < dest_size - 1; i++)
        if (!(dest[i] = fscan_read_gexptr(fchp->ptrs_fp, fchp->offset, errbufp)) ||
            dest[i] >= fchp->size + fchp->offset - 4) {
            dest[i] = 0;
            return i;
        }
    dest[dest_size - 2] = 0;
    return dest_size - 1;
}

inline static void p_read_arr_of_bmp_ptrs_and_push_valid_bmp_offs_to_vec(fscan_file_chunk fchp[static 1],
                                                                         gexdev_u32vec vecp[static 1],
                                                                         jmp_buf(*errbufp))
{
    u32 bmp_offsets[256] = {0};
    fscan_read_gexptr_null_term_arr(fchp, bmp_offsets, 256, errbufp);
    for (int ii = 0; ii < 256 && bmp_offsets[ii]; ii++) {
        u16 wh[2] = {0};
        fseek(fchp->data_fp, bmp_offsets[ii], SEEK_SET);
        if (fread_LE_U16(wh, 2, fchp->data_fp) != 2)
            longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

        if (wh[0] && wh[1])
            gexdev_u32vec_push_back(vecp, bmp_offsets[ii]);
    }
}

const gexdev_u32vec *fscan_search_for_ext_bmps(fscan_files *files_stp)
{
    u32 block_offsets[6] = {0};
    jmp_buf *errbufp = files_stp->error_jmp_buf;

    if (files_stp->ext_bmp_offsets.size || !files_stp->bitmap_chunk.ptrs_fp)
        return &files_stp->ext_bmp_offsets;

    fseek(files_stp->bitmap_chunk.ptrs_fp, files_stp->bitmap_chunk.ep, SEEK_SET);

    for (int i = 0; i < 6; i++) {
        block_offsets[i] = fscan_read_gexptr(files_stp->bitmap_chunk.ptrs_fp, files_stp->bitmap_chunk.offset, errbufp);
    }

    for (int i = 0; i < 6; i++) {
        if (block_offsets[i]) {
            fseek(files_stp->bitmap_chunk.ptrs_fp, block_offsets[i], SEEK_SET);
            p_read_arr_of_bmp_ptrs_and_push_valid_bmp_offs_to_vec(&files_stp->bitmap_chunk,
                                                                  &files_stp->ext_bmp_offsets,
                                                                  errbufp);
        }
    }
    return &files_stp->ext_bmp_offsets;
}

const gexdev_u32vec *fscan_search_for_tile_bmps(fscan_files *files_stp)
{
    u32 block_offsets[8] = {0};
    jmp_buf *errbufp = files_stp->error_jmp_buf;

    if (files_stp->tile_bmp_offsets.size || !files_stp->bitmap_chunk.ptrs_fp)
        return &files_stp->tile_bmp_offsets;

    fseek(files_stp->bitmap_chunk.ptrs_fp, files_stp->bitmap_chunk.ep, SEEK_SET);
    fscan_read_gexptr_null_term_arr(&files_stp->bitmap_chunk, block_offsets, sizeofarr(block_offsets), errbufp);

    for (int i = 0; i < sizeofarr(block_offsets) && block_offsets[i]; i++) {
        fseek(files_stp->bitmap_chunk.ptrs_fp, block_offsets[i], SEEK_SET);
        p_read_arr_of_bmp_ptrs_and_push_valid_bmp_offs_to_vec(&files_stp->bitmap_chunk,
                                                              &files_stp->tile_bmp_offsets,
                                                              errbufp);
    }
    return &files_stp->tile_bmp_offsets;
}

// TODO: Make more suitable for fscan_tiles_scan
void p_fscan_add_offset_to_loc_vec(fscan_files *files_stp, fscan_file_chunk *fchp, gexdev_univec *vecp,
                                   const uint *iters)
{
    u32 *extind = &files_stp->ext_bmp_counter;
    u32 type = 0;
    fscan_gfx_loc_info gfx_loc_info = {ftell(fchp->ptrs_fp), ~0,
                                       {iters[3], iters[2], iters[1], iters[0]}};

    fseek(fchp->ptrs_fp, 8, SEEK_CUR);
    fscan_read_gexptr_and_follow(fchp, 16, files_stp->error_jmp_buf);

    if (!fread_LE_U32(&type, 1, fchp->ptrs_fp))
        longjmp(*files_stp->error_jmp_buf, FSCAN_READ_ERROR_FREAD);

    if ((type & 0xF0) == 0xC0) {
        struct gex_gfxchunk gchunk = {0};
        // TODO: Check if bitmap is already in the list with ptr_map
        gfx_loc_info.ext_bmp_index = *extind;

        for (uint i = 0; i < IMG_CHUNKS_LIMIT; i++, (*extind)++) {
            if (!gex_gfxchunk_parsef(fchp->ptrs_fp, &gchunk)) break;
        }
    }

    gexdev_univec_push_back(vecp, &gfx_loc_info);
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
    if (fchp->ptrs_fp) {
        fclose(fchp->ptrs_fp);
        fchp->ptrs_fp = NULL;
    }
    if (fchp->data_fp) {
        fclose(fchp->data_fp);
        fchp->data_fp = NULL;
    }
}
