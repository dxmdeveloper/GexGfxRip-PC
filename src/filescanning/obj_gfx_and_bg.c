#include "obj_gfx_and_bg.h"
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../essentials/ptr_map.h"
#include "../helpers/binary_parse.h"

// TODO: DOCS OF THE PATTERNS
#define FSCAN_OBJ_GFX_FLW_PATTERN "e+0x20gg [G{ g [G{ [G{ +24 g [G{ c };]   };] };] }+4;]"
#define FSCAN_BACKGROUND_FLW_PATTERN "eg+24[G{ +48 [G{ +4 ggg   [G{+24g[G{ c };]};]   };] };]"

typedef void (*onfound_cb_t)(void *, const void *, const void *, const struct gfx_palette *, u32 *,
                             struct gfx_properties *);

typedef gexdev_u32vec vec32;
typedef gexdev_univec univec;

// _________________________________ static function declarations _________________________________
static u32 p_cb_bmp_header_binds_compute_index(const void *key);

// TODO: consider change
static int
p_prep_obj_gfx_and_exec_cb(fscan_files files_stp[1], fscan_file_chunk fchp[1], void *pass2cb, onfound_cb_t cb,
                           gexdev_ptr_map *bmp_headers_binds_mapp, uint *iterbufp, uint iters[4]);

/** @brief adds offsets of graphic entries (structure with properties, header and palette offsets) to vecp vector.
 *  Counts bitmaps from bitmap chunk used in graphic. Increments ext_bmp_counter if such bitmap is found.*/
static void p_add_offset_to_vec(fscan_files *files_stp, fscan_file_chunk *fchp, gexdev_univec *vecp,
                                const uint iters[4]);

inline static size_t p_scan_chunk_for_obj_gfx(fscan_files files_stp[1], fscan_file_chunk fchp[1], univec offvec[1]);

// _____________________________________ function definitions _____________________________________

static u32 p_cb_bmp_header_binds_compute_index(const void *key) {
    return *(const u32 *) key / 32;
}

size_t fscan_obj_gfx_scan(fscan_files *files_stp) {
    if (!files_stp->main_chunk.ptrs_fp)
        return 0;

    if (files_stp->option_verbose)
        printf("------------- object scan -------------\n");

    files_stp->ext_bmp_counter = 0;
    files_stp->obj_gfx_offsets.size = 0;

    return p_scan_chunk_for_obj_gfx(files_stp, &files_stp->main_chunk, NULL);
}

size_t fscan_intro_obj_gfx_scan(fscan_files *files_stp) {
    if (!files_stp->intro_chunk.ptrs_fp)
        return 0;

    if (files_stp->option_verbose)
        printf("----------- intro object scan ----------\n");

    // Scan main chunk before if not scanned yet
    if (!files_stp->obj_gfx_offsets.size || files_stp->intro_gfx_offsets.size) {
        fscan_obj_gfx_scan(files_stp);
    }
    files_stp->obj_gfx_offsets.size = 0;

    return p_scan_chunk_for_obj_gfx(files_stp, &files_stp->intro_chunk, NULL);
}

size_t fscan_background_scan(fscan_files *files_stp) {
    if (!files_stp->bg_chunk.ptrs_fp)
        return 0;

    jmp_buf **errbufpp = &files_stp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, fprintf(stderr, "fscan_background_scan error\n");)

    if (files_stp->option_verbose)
        printf("------------ background scan ------------\n");

    if (files_stp->obj_gfx_offsets.size == 0)
        fscan_obj_gfx_scan(files_stp);

    if (files_stp->intro_gfx_offsets.size == 0)
        fscan_intro_obj_gfx_scan(files_stp);

    files_stp->bg_gfx_offsets.size = 0;

    // Scan background file chunk
    size_t total = 0;
    fscan_file_chunk *bgchp = &files_stp->bg_chunk;
    FILE *bgfp = bgchp->ptrs_fp;

    // TODO: scanning implementation
    // "eg+24[G{ +48 [G{ +4 ggg   [G{+24g[G{ c };]};]   };] };]"
    fseek(bgfp, bgchp->ep, SEEK_SET);
    fscan_read_gexptr_and_follow(bgchp, 24, *errbufpp);

    u32 layers_offs[256] = {0};
    fscan_read_gexptr_null_term_arr(bgchp, layers_offs, sizeofarr(layers_offs), *errbufpp);

    for (uint i = 0; i < sizeofarr(layers_offs) && layers_offs[i]; i++) {
        fseek(bgfp, layers_offs[i] + 48, SEEK_SET);

        u32 anim_sets[256] = {0}; // I don't remember what it really is
        fscan_read_gexptr_null_term_arr(bgchp, anim_sets, sizeofarr(anim_sets), *errbufpp);

        for (uint ii = 0; ii < sizeofarr(anim_sets) && anim_sets[ii]; ii++) {
            fseek(bgfp, anim_sets[ii] + 4, SEEK_SET);

            fscan_read_gexptr_and_follow(bgchp, 0, *errbufpp);
            fscan_read_gexptr_and_follow(bgchp, 0, *errbufpp);
            fscan_read_gexptr_and_follow(bgchp, 0, *errbufpp);

            u32 anim_frames[256] = {0};
            fscan_read_gexptr_null_term_arr(bgchp, anim_frames, sizeofarr(anim_frames), *errbufpp);

            for (uint iii = 0; iii < sizeofarr(anim_frames) && anim_frames[iii]; iii++) {
                fseek(bgfp, anim_frames[iii] + 24, SEEK_SET);
                fscan_read_gexptr_and_follow(bgchp, 0, *errbufpp);

                u32 comb_gfx[256] = {0};
                fscan_read_gexptr_null_term_arr(bgchp, comb_gfx, sizeofarr(comb_gfx), *errbufpp);

                for (uint iv = 0; iv < sizeofarr(comb_gfx) && comb_gfx[iv]; iv++) {
                    uint iters[4] = {i, ii, iii, iv};
                    p_add_offset_to_vec(files_stp, bgchp, &files_stp->bg_gfx_offsets, iters);
                    total++;
                }
            }
        }
    }

    FSCAN_ERRBUF_REVERT(errbufpp);
    return total;
}

inline static size_t p_scan_chunk_for_obj_gfx(fscan_files files_stp[1], fscan_file_chunk fchp[1], univec offvec[1]) {
    jmp_buf **errbufpp = &files_stp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, fprintf(stderr, "p_scan_chunk_for_obj_gfx error\n");)

    // search for bitmaps in bitmap chunk. The function below will not rescan the chunk if it was already scanned
    fscan_search_for_ext_bmps(files_stp);

    /* main chunk scan start */
    size_t total = 0;
    u32 obj_offsets[256] = {0};

    fseek(fchp->ptrs_fp, fchp->ep + 0x20, SEEK_SET);

    fscan_read_gexptr_and_follow(fchp, 0, *errbufpp);
    fscan_read_gexptr_and_follow(fchp, 0, *errbufpp);

    fscan_read_gexptr_null_term_arr(fchp, obj_offsets, sizeof(obj_offsets), *errbufpp);

    for (uint i = 0; i < sizeofarr(obj_offsets) && obj_offsets[i]; i++) {
        // omit offsets of odd iterations
        if (i % 2)
            continue;

        fseek(fchp->ptrs_fp, obj_offsets[i], SEEK_SET);
        fscan_read_gexptr_and_follow(fchp, 0, *errbufpp);

        u32 anim_set_offs[128] = {0};
        fscan_read_gexptr_null_term_arr(fchp, anim_set_offs, sizeofarr(anim_set_offs), *errbufpp);

        for (uint ii = 0; ii < sizeofarr(anim_set_offs) && anim_set_offs[ii]; ii++) {
            fseek(fchp->ptrs_fp, anim_set_offs[ii], SEEK_SET);

            u32 anim_frame_offs[128] = {0};
            fscan_read_gexptr_null_term_arr(fchp, anim_frame_offs, sizeofarr(anim_frame_offs), *errbufpp);

            for (uint iii = 0; iii < sizeofarr(anim_frame_offs) && anim_frame_offs[iii]; iii++) {
                fseek(fchp->ptrs_fp, anim_frame_offs[iii] + 24, SEEK_SET);
                fscan_read_gexptr_and_follow(fchp, 0, *errbufpp);

                u32 combined_gfx_offs[128] = {0};
                fscan_read_gexptr_null_term_arr(fchp, combined_gfx_offs, sizeofarr(combined_gfx_offs), *errbufpp);

                for (uint iv = 0; iv < sizeofarr(combined_gfx_offs) && combined_gfx_offs[iv]; iv++) {
                    uint iters[4] = {i, ii, iii, iv};

                    fseek(fchp->ptrs_fp, combined_gfx_offs[iv], SEEK_SET);
                    p_add_offset_to_vec(files_stp, fchp, offvec, iters);
                    total++;
                }
            }
        }
    }

    // cleanup
    FSCAN_ERRBUF_REVERT(errbufpp);
    return total;
}

static int
p_prep_obj_gfx_and_exec_cb(fscan_files files_stp[1], fscan_file_chunk fchp[1], void *pass2cb, onfound_cb_t cb,
                           gexdev_ptr_map *bmp_headers_binds_mapp, uint *iterbufp, uint iters[4]) {
    fscan_file_chunk *main_chp = fchp;
    fscan_file_chunk *bmp_chp = &files_stp->bitmap_chunk;
    jmp_buf **errbufpp = &files_stp->error_jmp_buf;

    void *header_and_bmp = NULL;
    void *bmpp = NULL;
    size_t gfx_size = 0;
    u32 gfx_flags = 0;
    struct gfx_palette pal = {0};
    struct gfx_properties gfx_props = {0};

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, fprintf(stderr, "p_prep_obj_gfx_and_exec_cbfread error\n"); if (header_and_bmp)
        free(header_and_bmp);)

    // graphic properties read
    fread_LE_U16(&gfx_props.pos_y, 1, main_chp->ptrs_fp);
    fread_LE_U16(&gfx_props.pos_x, 1, main_chp->ptrs_fp);
    fread_LE_U32(&gfx_flags, 1, main_chp->ptrs_fp);
    gfx_props.is_flipped_vertically = gfx_flags & (1 << 7);
    gfx_props.is_flipped_horizontally = gfx_flags & (1 << 6);

    // print information
    if (files_stp->option_verbose) {
        u32 header_off = fscan_read_gexptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp);
        u32 pal_off = fscan_read_gexptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp);
        char type;
        if (!header_off)
            return 1;
        fseek(fchp->data_fp, header_off + 16, SEEK_SET);
        if (!fread(&type, 1, 1, fchp->data_fp))
            return 1;
        printf("found image at: %lx  {\"type\": %0hhX, \"header offset\": %x, \"palette offset\": %x",
               ftell(fchp->ptrs_fp) - 16, type,
               header_off, pal_off);
        if ((type & 0xF0) == 0xC0) {
            printf(",\"ext_bmp_counter\": %u", files_stp->ext_bmp_counter);
        }
        printf("} iterVec: [");
        for (size_t i = 0; i < 3; i++) {
            printf("%u, ", iters[i]);
        }
        printf("%u]\n", iters[3]);
        fseek(main_chp->ptrs_fp, -8, SEEK_CUR);
    }

    gfx_size = fscan_read_header_and_bitmaps_alloc(main_chp, bmp_chp, &header_and_bmp, &bmpp,
                                                   files_stp->ext_bmp_offsets.v,
                                                   files_stp->ext_bmp_offsets.size, &files_stp->ext_bmp_counter,
                                                   *errbufpp,
                                                   bmp_headers_binds_mapp);
    if (gfx_size == 0) {
        FSCAN_ERRBUF_REVERT(errbufpp);
        return 1;
    }

    // palette parse
    u32 paletteOffset = fscan_read_gexptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp);

    fseek(main_chp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(main_chp->data_fp, &pal);

    // finish reading properties
    fread_LE_U32(&gfx_flags, 1, main_chp->ptrs_fp);
    gfx_flags = gfx_flags >> 16;
    gfx_props.is_semi_transparent = gfx_flags & (1 << 15);
    // TODO: Investigate for more properties

    // callback call
    cb(pass2cb, header_and_bmp, bmpp, &pal, iters, &gfx_props);

    //cleanup
    free(header_and_bmp);

    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

void p_add_offset_to_vec(fscan_files *files_stp, fscan_file_chunk *fchp, gexdev_univec *vecp, const uint iters[4]) {
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
