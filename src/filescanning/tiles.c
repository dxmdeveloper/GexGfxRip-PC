#include "tiles.h"
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"
#include "../essentials/ptr_map.h"
#include "../helpers/basicdefs.h"

//  -------------- STATIC DECLARATIONS --------------

/** @brief made as callback for gexdev_ptr_map to shrink size of tile header pointers table */
static u32 p_cb_tile_header_binds_compute_index(const void *key);

/** @brief specific use function. Use as fscan_follow_pattern_recur's callback while scanning for tiles. 
  * @param tile_bmp_offsets_vecp array of 2 u32 vectors. First vector points where offsets of each block starts in second vector
  * @param bmp_index zero initialized array of 32 uints. Function keeps information from previous calls */
static int
p_prep_tile_gfx_data_and_exec_cb(fscan_files files_stp[1], u32 tile_gfx_id, uint tile_anim_frame, uint block_ind,
                                 gexdev_ptr_map *bmp_headers_binds_map, const gexdev_u32vec tile_bmp_offsets_vecp[2],
                                 uint bmp_index[32], void *pass2cb,
                                 void cb(void *, const void *, const void *, const struct gfx_palette *, u16, u16));

// ---------------- FUNC DEFINITIONS ----------------

// TODO: reimplement this like fscan_obj_gfx_scan. Collect offsets first.
size_t fscan_tiles_scan(fscan_files files_stp[static 1]) {
    fscan_file_chunk *tchp = &files_stp->tile_chunk;
    fscan_file_chunk *mchp = &files_stp->main_chunk;
    size_t total = 0;

    // ---------------------- error handling ----------------------
    jmp_buf **errbufpp = &files_stp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, fprintf(stderr, "fscan_tiles_scan error");)
    // -----------------------------------------------------------

    // ----- tile file chunk scan for bitmap offsets -----
    fscan_search_for_tile_bmps(files_stp);

    // ----- main file chunk scan for graphic entries -----
    uint bmp_iters[32] = {0};
    u32 block[32] = {0};

    fseek(mchp->ptrs_fp, mchp->ep + 0x28, SEEK_SET);
    fscan_read_gexptr_and_follow(mchp, 0, *errbufpp);

    // read offsets of tile gfx blocks
    fscan_read_gexptr_null_term_arr(mchp, block, 32, *errbufpp);

    // follow the read offsets, prepare tile graphics and call the callback passed to current function.
    for (uint i = 0; i < 32 && block[i]; i++) {
        u32 gfxid = 0;
        u32 animsetsoff = 0;

        fseek(mchp->ptrs_fp, block[i], SEEK_SET);
        animsetsoff = fscan_read_gexptr(mchp->ptrs_fp, mchp->offset, *errbufpp);

        if (animsetsoff >= mchp->size + mchp->offset - 4)
            longjmp(**errbufpp, FSCAN_READ_ERROR_INVALID_POINTER);

        // base tile graphics
        // TODO: similar to fscan_obj_gfx_scan. Collect offsets
        fread_LE_U32(&gfxid, 1, mchp->ptrs_fp); // read tile graphic id
        while(gfxid <= 0xffff) {
            fseek(mchp->ptrs_fp, -4, SEEK_CUR);
            p_fscan_add_offset_to_loc_vec(files_stp, mchp, &files_stp->tile_gfx_offsets, bmp_iters);
            fseek(mchp->ptrs_fp, 4, SEEK_CUR); // TODO: Ensure that this is correct
            fread_LE_U32(&gfxid, 1, mchp->ptrs_fp); // read next tile graphic id
        }


        // animated tiles
        uint animind = 0;
        u32 aframesoff = 0;
        if (!animsetsoff)
            continue;

        fseek(mchp->ptrs_fp, animsetsoff, SEEK_SET);
        while ((aframesoff = fscan_read_gexptr(mchp->ptrs_fp, mchp->offset, *errbufpp))) {
            uint animfrind = 1;

            fread_LE_U32(&gfxid, 1, mchp->ptrs_fp); // read graphic id
            if (aframesoff >= mchp->size + mchp->offset - 4)
                longjmp(**errbufpp, FSCAN_READ_ERROR_INVALID_POINTER);
            fseek(mchp->ptrs_fp, aframesoff, SEEK_SET);

            while (p_prep_tile_gfx_data_and_exec_cb(files_stp, gfxid, animfrind, i, &bmp_headers_binds_map,
                                                    tile_bmp_offsets, bmp_iters,
                                                    pass2cb, cb)) {
                animfrind++;
            }

            fseek(mchp->ptrs_fp, animsetsoff + 20 * ++animind, SEEK_SET);
        }
    }

    FSCAN_ERRBUF_REVERT(errbufpp);
    return total;
}

static int
p_prep_tile_gfx_data_and_exec_cb(fscan_files files_stp[1], u32 tile_gfx_id, uint tile_anim_frame, uint block_ind,
                                 gexdev_ptr_map *bmp_headers_binds_map, const gexdev_u32vec tile_bmp_offsets_vecp[2],
                                 uint bmp_index[32], void *pass2cb,
                                 void cb(void *, const void *, const void *, const struct gfx_palette *, u16, u16)) {
    void *header_and_bmp = NULL;
    void *bmpp = NULL;
    size_t gfx_size = 0;
    struct gfx_palette pal = {0};
    fscan_file_chunk *mchp = &files_stp->main_chunk;
    fscan_file_chunk *tchp = &files_stp->tile_chunk;
    jmp_buf **errbufpp = &files_stp->error_jmp_buf;

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, {
        fprintf(stderr, "p_prep_tile_gfx_data_and_exec_cb error\n");
        fprintf(stderr, "main file chunk: ptrs_fp pos=%lu, data_fp pos=%lu\n", ftell(mchp->ptrs_fp),
                ftell(mchp->data_fp));
        if (header_and_bmp)
            free(header_and_bmp);
    })

    if (tile_gfx_id > 0xffff || block_ind >= 32) {
        // end of tile list
        FSCAN_ERRBUF_REVERT(errbufpp);
        return 0;
    }

    u32 hoff;
    if (!(hoff = fscan_read_gexptr(mchp->ptrs_fp, mchp->offset, *errbufpp)) || hoff > mchp->size + mchp->offset) {
        FSCAN_ERRBUF_REVERT(errbufpp);
        return 0;
    }

    fseek(mchp->ptrs_fp, -4, SEEK_CUR);

    if (tile_bmp_offsets_vecp[0].size <= block_ind ||
        tile_bmp_offsets_vecp[1].size <= tile_bmp_offsets_vecp[0].v[block_ind])
        longjmp(**errbufpp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

    gfx_size = fscan_read_header_and_bitmaps_alloc(mchp, tchp, &header_and_bmp, &bmpp,
                                                   &tile_bmp_offsets_vecp[1].v[tile_bmp_offsets_vecp[0].v[block_ind]],
                                                   tile_bmp_offsets_vecp[1].size, &bmp_index[block_ind], *errbufpp,
                                                   bmp_headers_binds_map);
    if (!gfx_size) {
        FSCAN_ERRBUF_REVERT(errbufpp);
        fseek(mchp->ptrs_fp, 8, SEEK_CUR);
        return 1;
    }

    // palette read
    // TODO: CACHE PALETTE
    u32 paletteOffset = fscan_read_gexptr(mchp->ptrs_fp, mchp->offset, *errbufpp);
    fseek(mchp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mchp->data_fp, &pal);

    // CALLING ON FOUND CALLBACK
    cb(pass2cb, header_and_bmp, bmpp, &pal, (u16) tile_gfx_id, tile_anim_frame);

    // omit 4 bytes (some flags, but only semi transparency affects tile graphics as far as I know)
    fseek(mchp->ptrs_fp, 4, SEEK_CUR);

    // cleanup
    free(header_and_bmp);
    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

static u32 p_cb_tile_header_binds_compute_index(const void *key) {
    const u32 *u32key = key;
    return *u32key / 32;
}
