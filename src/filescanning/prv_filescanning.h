#pragma once
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../helpers/binary_parse.h"
#include <setjmp.h>


//  -------------- STATIC DECLARATIONS --------------
/** @return 0 on success, negative value on error */
inline static int p_fscan_collect_gfx_info_common_part(fscan_files files_stp[static 1],
                                                       fscan_file_chunk *fchp,
                                                       size_t used_gfx_map_ind,
                                                       gexdev_u32vec ext_bmp_offsets[static 1],
                                                       const fscan_gfx_info_vec ginfv[static 1],
                                                       jmp_buf *errbufp,
                                                       u32 ext_bmp_counter[static 1],
                                                       fscan_gfx_info ginf[static 1]);

// ---------------- FUNC DEFINITIONS ----------------
inline static int p_fscan_collect_gfx_info_common_part(fscan_files files_stp[static 1],
                                                       fscan_file_chunk *fchp,
                                                       size_t used_gfx_map_ind,
                                                       gexdev_u32vec ext_bmp_offsets[static 1],
                                                       const fscan_gfx_info_vec ginfv[static 1],
                                                       jmp_buf *errbufp,
                                                       u32 ext_bmp_counter[static 1],
                                                       fscan_gfx_info ginf[static 1])
{
    u32 extind = *ext_bmp_counter;
    u32 type = 0;
    u32 gfx_flags = 0;
    long saved_pos = 0;

    // header offset read
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
    // ! DEBUG
    printf("gfxoff: %d\n", gfxoff);

    // read palette offset
    fseek(fchp->fp, saved_pos, SEEK_SET);
    ginf->palette_offset = fscan_read_gexptr(fchp->fp, 0, errbufp);

    // finish reading properties
    fread_LE_U32(&gfx_flags, 1, fchp->fp);
    gfx_flags = gfx_flags >> 16;
    ginf->gfx_props.is_semi_transparent = gfx_flags & (1 << 15);

    // go to the graphic location
    fseek(fchp->fp, gfxoff + 16, SEEK_SET);

    // read type signature
    fread_LE_U32(&type, 1, fchp->fp);

    // count gfx chunks
    struct gex_gfxchunk gchunk = {0};
    for (; ginf->chunk_count < IMG_CHUNKS_LIMIT; ginf->chunk_count++) {
        gex_gfxchunk_parsef(fchp->fp, &gchunk);
        if (gchunk.width == 0) break;
    }

    if ((type & 0xF0) == 0xC0
        && gexdev_bitflag_arr_get(&files_stp->used_gfx_flags[used_gfx_map_ind], (gfxoff - fchp->offset) / 8)
            == 0) /* graphic not used before */
    {
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

        // allocate memory for ext_bmp_offsets + shared pointer counter
        ginf->ext_bmp_offsets = malloc(ginf->chunk_count * sizeof(u32) + sizeof(size_t));
        if (!ginf->ext_bmp_offsets)
            exit(0xbeef);

        // copy ext_bmp_offsets from ext_bmp_offsets
        memcpy(ginf->ext_bmp_offsets, ext_bmp_offsets->v + extind, ginf->chunk_count * sizeof(u32));

        // set shared pointer counter to 1
        *(size_t *) (ginf->ext_bmp_offsets + ginf->chunk_count) = 1;

    } else if (((type & 0xF0) == 0xC0)) {
        // find the gfx info with the same gfx_offset and copy the ext_bmp_offsets, gfx_props and chunk_count
        fscan_gfx_info *to_copy = NULL;
        for (int i = (int) ginfv->size - 1; i >= 0; i--) {
            fscan_gfx_info *info = fscan_gfx_info_vec_at(ginfv, i);
            if (info->gfx_offset == gfxoff) {
                to_copy = fscan_gfx_info_vec_at(ginfv, i);
                break;
            }
        }
        if (!to_copy)
            exit(0xeeee); // should never happen

        ginf->ext_bmp_offsets = to_copy->ext_bmp_offsets;
        ginf->gfx_props = to_copy->gfx_props;
        ginf->chunk_count = to_copy->chunk_count;
        // increment shared pointer counter
        (*(size_t *) (to_copy->ext_bmp_offsets + to_copy->chunk_count)) += 1;
    }

    // set used flag
    gexdev_bitflag_arr_set(&files_stp->used_gfx_flags[used_gfx_map_ind], (gfxoff - fchp->offset) / 8, 1);
    return 0;
}

