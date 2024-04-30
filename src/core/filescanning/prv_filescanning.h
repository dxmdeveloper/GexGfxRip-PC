#pragma once
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../helpers/binary_parse.h"
#include <setjmp.h>


//  -------------- STATIC DECLARATIONS --------------
/** @return 0 on success, negative value on error */
inline static int p_fscan_collect_gfx_info_common_part(fscan_files files_stp[static 1],
                                                       fscan_file_chunk fchp[static 1],
                                                       gexdev_bitflag_arr used_gfx_map[static 1],
                                                       gexdev_u32vec ext_bmp_offsets[static 1],
                                                       const fscan_gfx_info_vec *ginfv,
                                                       jmp_buf *errbufp,
                                                       u32 ext_bmp_counter[static 1],
                                                       fscan_gfx_info ginf[static 1]);

// ---------------- FUNC DEFINITIONS ----------------
inline static int p_fscan_collect_gfx_info_common_part(fscan_files files_stp[static 1],
                                                       fscan_file_chunk fchp[static 1],
                                                       gexdev_bitflag_arr used_gfx_map[static 1],
                                                       gexdev_u32vec ext_bmp_offsets[static 1],
                                                       const fscan_gfx_info_vec *ginfv,
                                                       jmp_buf *errbufp,
                                                       u32 ext_bmp_counter[static 1],
                                                       fscan_gfx_info ginf[static 1])
{
    u32 extind = *ext_bmp_counter;
    u32 type = 0;
    u32 gfx_flags = 0;
    long saved_pos = 0;

    // header offset read
    saved_pos = ftell(fchp->fp);
    u32 gfxoff = ginf->gfx_offset = fscan_read_gexptr(fchp->fp, fchp->offset, files_stp->error_jmp_buf);

    if (!gfxoff || !fread_LE_U32(&type, 1, fchp->fp)) {
        if (errbufp) {
            dbg_errlog("Graphic offset out of range or file read error\n");
            longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);
        } else {
            ginf->gfx_offset = 0;
            return -1;
        }
    }

    if (ginfv) {
        // read palette offset
        fseek(fchp->fp, saved_pos + 4, SEEK_SET);

        ginf->palette_offset = fscan_read_gexptr(fchp->fp, fchp->offset, errbufp);

        // finish reading properties
        fread_LE_U32(&gfx_flags, 1, fchp->fp);
        gfx_flags = gfx_flags >> 16;
        ginf->gfx_props.is_semi_transparent = gfx_flags & (1 << 15);
    }

    // go to the graphic location
    fseek(fchp->fp, gfxoff + 16, SEEK_SET);

    // read type signature
    fread_LE_U32(&type, 1, fchp->fp);

    // count gfx chunks
    u8 chunkaob[(IMG_CHUNKS_LIMIT + 1) * 8] = {0};
    struct gex_gfxchunk gchunk = {0};

    for (size_t i = 0; i < IMG_CHUNKS_LIMIT; i++) {
        fread(chunkaob+i*8, 8, 1, fchp->fp);
        gchunk = gex_gfxchunk_parse_aob(chunkaob+i*8);
        if (gchunk.width == 0) break;
        ginf->chunk_count++;
    }

    // calculate real width and height
    u32 wh[2] = {0};
    gfx_calc_real_width_and_height(&wh[0], &wh[1], chunkaob);
    ginf->width = wh[0];
    ginf->height = wh[1];

    if ((type & 0xF0) == 0xC0) {
        // if graphic wasn't used before
        if (gexdev_bitflag_arr_get(used_gfx_map, (gfxoff - fchp->offset) / 32)
            == 0) {
            // check if there are enough offsets in the ext_bmp_offsets array
            if (extind + ginf->chunk_count > ext_bmp_offsets->size) {
                if (errbufp)
                    longjmp(*errbufp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);
                else {
                    ginf->gfx_offset = 0;
                    return -2;
                }
            }
            // ext_bmp_counter increment
            (*ext_bmp_counter) += ginf->chunk_count;
            // set used flag
            gexdev_bitflag_arr_set(used_gfx_map, (gfxoff - fchp->offset) / 32, 1);
        }

        if (ginfv) {
            ginf->ext_bmp_offsets = malloc(ginf->chunk_count * sizeof(u32));
            if (!ginf->ext_bmp_offsets)
                exit(0xbeef);

            // copy ext_bmp_offsets from ext_bmp_offsets
            memcpy(ginf->ext_bmp_offsets, ext_bmp_offsets->v + extind, ginf->chunk_count * sizeof(u32));

        }
    }

    return 0;
}

