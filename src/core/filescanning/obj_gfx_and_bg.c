#include "obj_gfx_and_bg.h"
#include "filescanning.h"
#include "prv_filescanning.h"

// Leftovers from old implementation
#define FSCAN_OBJ_GFX_FLW_PATTERN "e+0x20gg [G{ g [G{ [G{ +24 g [G{ c };]   };] };] }+4;]"
#define FSCAN_BACKGROUND_FLW_PATTERN "eg+24[G{ +48 [G{ +4 ggg   [G{+24g[G{ c };]};]   };] };]"

typedef void (*onfound_cb_t)(void *, const void *, const void *, const struct gfx_palette *, u32 *,
                             struct gfx_properties *);

typedef gexdev_univec univec;

// _________________________________ static function declarations _________________________________
//static u32 p_cb_bmp_header_binds_compute_index(const void *key);

inline static int p_scan_chunk_for_obj_gfx(fscan_files sf[1], fscan_file_chunk fchp[1], fscan_gfx_info_vec *ginfv);

/// @brief Collects graphic information from file chunk and returns it as fscan_gfx_info object.
/// Uses files_stp->ext_bmp_counter to count external bitmaps.
static inline fscan_gfx_info p_collect_gfx_info(fscan_files files_stp[static 1],
                                                fscan_file_chunk fchp[static 1],
                                                const u8 iter[static 4],
                                                gexdev_bitflag_arr *used_gfx_map,
                                                const fscan_gfx_info_vec *ginfv,
                                                jmp_buf (*errbufp));

// _____________________________________ function definitions _____________________________________

//static u32 p_cb_bmp_header_binds_compute_index(const void *key)
//{
//    return *(const u32 *) key / 32;
//}

int fscan_obj_gfx_scan(struct fscan_files *sf, fscan_gfx_info_vec *res_vec)
{
    if (!fscan_files_is_chunk_existing(sf, FCH_TYPE_MAIN))
        return -1;

    if (!res_vec && !fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS))
        return 0; // res_vec can be NULL in order to count used external bitmaps

    // reset ext_bmp_counter
    sf->ext_bmp_counter = 0;

    // don't close the bitflag array because we will need it later
    p_scan_chunk_for_obj_gfx(sf, &sf->file_chunks[FCH_TYPE_MAIN], res_vec);
    sf->last_scanned_chunk = 0;
    return 0;
}

int fscan_intro_obj_gfx_scan(struct fscan_files *sf, fscan_gfx_info_vec *res_vec)
{
    if (!fscan_files_is_chunk_existing(sf, FCH_TYPE_INTRO))
        return -1;

    if (!res_vec && !fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS))
        return 0; // res_vec can be NULL in order to count used external bitmaps

    // Scan main chunk before if not scanned yet to correctly set the ext_bmp_counter
    if (sf->last_scanned_chunk != 0 && fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS)) {
        fscan_obj_gfx_scan(sf, NULL);
    }

    if (sf->option_verbose)
        printf("----------- intro object scan ----------\n");


    p_scan_chunk_for_obj_gfx(sf, &sf->file_chunks[FCH_TYPE_INTRO], res_vec);
    sf->last_scanned_chunk = 1;
    return 0;
}

inline static int p_scan_chunk_for_obj_gfx(fscan_files *sf, fscan_file_chunk *fchp, fscan_gfx_info_vec *ginfv)
{
    gexdev_bitflag_arr used_gfx_map = {0};

    if (fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS)) {
        gexdev_bitflag_arr_create(&used_gfx_map, fchp->size / 32);
    }

    // error handling
    jmp_buf errbuf;
    jmp_buf *errbufp = &errbuf;
    int err = 0;
    if (setjmp(errbuf)) {
        gexdev_bitflag_arr_close(&used_gfx_map);
        dbg_errlog("p_scan_chunk_for_obj_gfx error\n");
        return err;
    }

    // search for bitmaps in bitmap chunk. The function below will not rescan the chunk if it was already scanned
    fscan_search_for_ext_bmps(sf);

    /* main chunk scan start */
    u32 obj_offsets[256] = {0};

    //e+0x20gg [G{ g [G{ [G{ +24 g [G{ c };]   };] };] }+4;]
    fseek(fchp->fp, fchp->ep + 0x20, SEEK_SET);

    fscan_read_gexptr_and_follow(fchp, 0, errbufp);
    fscan_read_gexptr_and_follow(fchp, 0, errbufp);

    fscan_read_gexptr_null_term_arr(fchp, obj_offsets, sizeof(obj_offsets), errbufp);

    for (uint i = 0; i < sizeofarr(obj_offsets) && obj_offsets[i]; i++) {
        if (i % 2) continue; // odd iterations skipped

        fseek(fchp->fp, obj_offsets[i], SEEK_SET);
        if (!fscan_read_gexptr_and_follow(fchp, 0, errbufp)) continue;

        u32 anim_set_offs[128] = {0};
        fscan_read_gexptr_null_term_arr(fchp, anim_set_offs, sizeofarr(anim_set_offs), errbufp);

        for (uint ii = 0; ii < sizeofarr(anim_set_offs) && anim_set_offs[ii]; ii++) {
            fseek(fchp->fp, anim_set_offs[ii], SEEK_SET);

            u32 anim_frame_offs[128] = {0};
            fscan_read_gexptr_null_term_arr(fchp, anim_frame_offs, sizeofarr(anim_frame_offs), errbufp);

            for (uint iii = 0; iii < sizeofarr(anim_frame_offs) && anim_frame_offs[iii]; iii++) {
                fseek(fchp->fp, anim_frame_offs[iii] + 24, SEEK_SET);
                if (!fscan_read_gexptr_and_follow(fchp, 0, errbufp)) continue;

                u32 combined_gfx_offs[128] = {0};
                fscan_read_gexptr_null_term_arr(fchp, combined_gfx_offs, sizeofarr(combined_gfx_offs), errbufp);

                for (uint iv = 0; iv < sizeofarr(combined_gfx_offs) && combined_gfx_offs[iv]; iv++) {
                    u8 it[4] = {(u8) i / 2 /* odd iterations skipped */, (u8) ii, (u8) iii, (u8) iv};
                    fseek(fchp->fp, combined_gfx_offs[iv], SEEK_SET);

                    fscan_gfx_info ginf = p_collect_gfx_info(sf, fchp, it,
                                                             &used_gfx_map, ginfv, errbufp);

                    if (ginfv)
                        gexdev_univec_push_back(&ginfv->base, &ginf);
                }
            }
        }
    }
    gexdev_bitflag_arr_close(&used_gfx_map);
    return 0;
}

int fscan_background_scan(struct fscan_files *sf, fscan_gfx_info_vec *res_vec)
{
    fscan_file_chunk *bgchp = &sf->file_chunks[FCH_TYPE_BACKGROUND];

    if (!fscan_files_is_chunk_existing(sf, FCH_TYPE_BACKGROUND))
        return -1;

    if (!res_vec && !fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS))
        return 0; // res_vec can be NULL in order to count used external bitmaps

    // create bitflag array of found graphics.
    gexdev_bitflag_arr used_gfx_map = {0};

    if (fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS)) {
        gexdev_bitflag_arr_create(&used_gfx_map, bgchp->size / 32);
    }

    // error handling
    jmp_buf errbuf;
    jmp_buf *errbufp = &errbuf;
    int err = 0;
    if ((err = setjmp(errbuf))) {
        gexdev_bitflag_arr_close(&used_gfx_map);
        dbg_errlog("fscan_background_scan error\n");
        return err;
    }

    // Scan main chunk and intro before if not scanned yet to correctly set the ext_bmp_counter
    if (sf->last_scanned_chunk != 1 && fscan_files_is_chunk_existing(sf, FCH_TYPE_EXT_BITMAPS)) {
        fscan_intro_obj_gfx_scan(sf, NULL);
    }

    // Scan background file chunk
    FILE *bgfp = bgchp->fp;

    fseek(bgfp, bgchp->ep, SEEK_SET);
    fscan_read_gexptr_and_follow(bgchp, 24, errbufp);

    u32 layers_offs[256] = {0};
    fscan_read_gexptr_null_term_arr(bgchp, layers_offs, sizeofarr(layers_offs), errbufp);

    for (uint i = 0; i < sizeofarr(layers_offs) && layers_offs[i]; i++) {
        fseek(bgfp, layers_offs[i] + 48, SEEK_SET);

        u32 anim_sets[256] = {0}; // I don't remember what it really is
        fscan_read_gexptr_null_term_arr(bgchp, anim_sets, sizeofarr(anim_sets), errbufp);

        for (uint ii = 0; ii < sizeofarr(anim_sets) && anim_sets[ii]; ii++) {
            fseek(bgfp, anim_sets[ii] + 4, SEEK_SET);

            fscan_read_gexptr_and_follow(bgchp, 0, errbufp);
            fscan_read_gexptr_and_follow(bgchp, 0, errbufp);
            fscan_read_gexptr_and_follow(bgchp, 0, errbufp);

            u32 anim_frames[256] = {0};
            fscan_read_gexptr_null_term_arr(bgchp, anim_frames, sizeofarr(anim_frames), errbufp);

            for (uint iii = 0; iii < sizeofarr(anim_frames) && anim_frames[iii]; iii++) {
                fseek(bgfp, anim_frames[iii] + 24, SEEK_SET);
                fscan_read_gexptr_and_follow(bgchp, 0, errbufp);

                u32 comb_gfx[256] = {0};
                fscan_read_gexptr_null_term_arr(bgchp, comb_gfx, sizeofarr(comb_gfx), errbufp);

                for (uint iv = 0; iv < sizeofarr(comb_gfx) && comb_gfx[iv]; iv++) {
                    u8 it[4] = {(u8) i, (u8) ii, (u8) iii, (u8) iv};
                    fscan_gfx_info ginf = p_collect_gfx_info(sf, bgchp, it, &used_gfx_map, res_vec, errbufp);
                    if (res_vec)
                        gexdev_univec_push_back(&res_vec->base, &ginf);
                }
            }
        }
    }

    gexdev_bitflag_arr_close(&used_gfx_map);
    sf->last_scanned_chunk = 2;
    return 0;
}

static inline
fscan_gfx_info p_collect_gfx_info(fscan_files files_stp[static 1], fscan_file_chunk fchp[static 1],
                                  const u8 iter[static 4], gexdev_bitflag_arr *used_gfx_map,
                                  const fscan_gfx_info_vec *ginfv, jmp_buf *errbufp)
{
    u32 gfx_flags = 0;
    fscan_gfx_info ginf = {.iteration = {iter[3], iter[2], iter[1], iter[0]}}; // little endian key

    //printf("offset: %lx\n", ftell(fchp->ptrs_fp));

    // graphic properties read
    fread_LE_I16(&ginf.gfx_props.pos_y, 1, fchp->fp);
    fread_LE_I16(&ginf.gfx_props.pos_x, 1, fchp->fp);
    fread_LE_U32(&gfx_flags, 1, fchp->fp);
    ginf.gfx_props.is_flipped_vertically = gfx_flags & (1 << 7);
    ginf.gfx_props.is_flipped_horizontally = gfx_flags & (1 << 6);

    p_fscan_collect_gfx_info_common_part(files_stp,
                                         fchp,
                                         used_gfx_map,
                                         &files_stp->ext_bmp_offsets,
                                         ginfv,
                                         errbufp,
                                         &files_stp->ext_bmp_counter,
                                         &ginf);
    return ginf;
}
