#include "tiles.h"
#include "filescanning.h"
#include "prv_filescanning.h"

//  -------------- STATIC DECLARATIONS --------------

/** @brief made as callback for gexdev_ptr_map to shrink size of tile header pointers table */
static u32 p_cb_tile_header_binds_compute_index(const void *key);

static inline
fscan_gfx_info p_collect_gfx_info(fscan_files files_stp[static 1],
                                  fscan_file_chunk fchp[static 1],
                                  const u8 iter[static 4],
                                  const fscan_gfx_info_vec *ginfv,
                                  gexdev_bitflag_arr used_gfx_map[static 1],
                                  jmp_buf (*errbufp),
                                  u32 ext_bmp_counter[static 1]);

// ---------------- FUNC DEFINITIONS ----------------

int fscan_tiles_scan(struct fscan_files *files_stp, fscan_gfx_info_vec *res_vec)
{
    fscan_file_chunk *mchp = &files_stp->main_chunk;

    if (!mchp->fp)
        return -1;

    if (!res_vec)
        return -2; // argument error

    // create bitflag array of found graphics.
    gexdev_bitflag_arr used_gfx_map = {0};

    if (files_stp->bitmap_chunk.fp) {
        gexdev_bitflag_arr_create(&used_gfx_map, files_stp->intro_chunk.size / 32);
    }

    // ---------------------- error handling ----------------------
    jmp_buf errbuf;
    jmp_buf *errbufp = &errbuf;
    int err;
    if ((err = setjmp(errbuf))) {
        gexdev_bitflag_arr_close(&used_gfx_map);
        dbg_errlog("fscan_tiles_scan error\n");
        return err;
    }
    // -----------------------------------------------------------

    // ----- tile file chunk scan for bitmap offsets -----
    fscan_search_for_tile_bmps(files_stp);

    // ----- main file chunk scan for graphic entries -----
    u32 block[32] = {0};

    fseek(mchp->fp, mchp->ep + 0x28, SEEK_SET);
    fscan_read_gexptr_and_follow(mchp, 0, errbufp);

    // read offsets of tile gfx blocks
    fscan_read_gexptr_null_term_arr(mchp, block, 32, errbufp);

    for (u8 i = 0; i < 32 && block[i]; i++) {
        u32 gfxid = 0;
        u32 tile_anims_off = 0;
        u32 extbmpcnt = 0;

        fseek(mchp->fp, block[i], SEEK_SET);
        tile_anims_off = fscan_read_gexptr(mchp->fp, mchp->offset, errbufp);

        if (tile_anims_off >= mchp->size + mchp->offset - 4)
            longjmp(*errbufp, FSCAN_READ_ERROR_INVALID_POINTER);

        // base tile graphics
        fread_LE_U32(&gfxid, 1, mchp->fp); // read tile graphic id
        while (gfxid <= 0xffff) {
            u8 it[4] = {i, (gfxid >> 8) & 0xff, gfxid & 0xff, 0};

            fscan_gfx_info
                ginf = p_collect_gfx_info(files_stp, mchp, it, res_vec, &used_gfx_map, errbufp, &extbmpcnt);

            if (ginf.gfx_offset == 0) // invalid graphic offset
                break;

            // add to vector
            gexdev_univec_push_back(res_vec, &ginf);
            fread_LE_U32(&gfxid, 1, mchp->fp); // read next tile graphic id
        }

        // animated tiles
        if (tile_anims_off) {
            uint anim_ind = 0;
            u32 aframeset_off = 0;

            fseek(mchp->fp, tile_anims_off, SEEK_SET);
            while ((aframeset_off = fscan_read_gexptr(mchp->fp, mchp->offset, errbufp))) {

                fread_LE_U32(&gfxid, 1, mchp->fp); // read graphic id
                if (aframeset_off >= mchp->size + mchp->offset - 4)
                    longjmp(*errbufp, FSCAN_READ_ERROR_INVALID_POINTER);

                // going to frames of tile animation
                fseek(mchp->fp, aframeset_off, SEEK_SET);

                uint aframe_ind = 1;
                u32 gfx_off = 0;
                fread_LE_U32(&gfx_off, 1, mchp->fp); // read first graphic offset
                while (gfx_off) {
                    u8 it[4] = {i, (gfxid >> 8) & 0xff, gfxid & 0xff, aframe_ind};

                    fseek(mchp->fp, -4, SEEK_CUR); // unread graphic offset
                    fscan_gfx_info
                        ginf = p_collect_gfx_info(files_stp, mchp, it, res_vec, &used_gfx_map, errbufp, &extbmpcnt);
                    gexdev_univec_push_back(res_vec, &ginf); // add to vector
                    fread_LE_U32(&gfx_off, 1, mchp->fp); // read next graphic offset
                    aframe_ind++;
                }

                fseek(mchp->fp, tile_anims_off + 20 * ++anim_ind, SEEK_SET);
            }
        }
    }

    gexdev_bitflag_arr_close(&used_gfx_map);
    return 0;
}

static u32 p_cb_tile_header_binds_compute_index(const void *key)
{
//    const u32 *u32key = key;
//    return *u32key / 32;
}

static inline fscan_gfx_info p_collect_gfx_info(fscan_files files_stp[static 1],
                                                fscan_file_chunk fchp[static 1],
                                                const u8 iter[static 4],
                                                const fscan_gfx_info_vec *ginfv,
                                                gexdev_bitflag_arr used_gfx_map[static 1],
                                                jmp_buf (*errbufp),
                                                u32 ext_bmp_counter[static 1])
{
    fscan_gfx_info ginf = {.iteration = {iter[3], iter[2], iter[1], iter[0]}}; // little endian key
    long saved_pos = ftell(fchp->fp);

    p_fscan_collect_gfx_info_common_part(files_stp, fchp, used_gfx_map,
                                         &files_stp->tile_ext_bmp_offsets[iter[0]],
                                         ginfv, errbufp, ext_bmp_counter, &ginf);

    // restore position and move to the next graphic
    fseek(fchp->fp, saved_pos + 12, SEEK_SET);

    return ginf;
}
