#include "filescanning_obj_gfx.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../essentials/ptr_map.h"
#include "../helpers/binary_parse.h"

struct obj_gfx_scan_pack {
    struct fscan_files * filesStp;
    void * pass2cb;
    void (*cb)(void * clientp, const void *bitmap, const void *headerAndOpMap,
               const struct gfx_palette *palette, gexdev_u32vec * itervecp);
    gexdev_ptr_map * bmp_headers_binds_mapp;
    gexdev_u32vec * ext_bmp_offsetsp;
    uint ext_bmp_index;
};

static u32 fscan_cb_bmp_header_binds_compute_index(const void* key){
    return *(const u32 *)key / 32;
}

static int fscan_cb_flwpat_push_offset(fscan_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 * internalVars, void * clientp){
    u32 offset = fscan_read_infile_ptr(fChunk->ptrs_fp, fChunk->offset, NULL);
    if(!offset) return 0;
    gexdev_u32vec_push_back(clientp, offset);
    return 1;
}

static int _fscan_prep_obj_gfx_and_exec_cb(fscan_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 *iv, void * clientp){
    struct obj_gfx_scan_pack * packp = clientp;
    fscan_file_chunk * mainChp = &packp->filesStp->main_chunk;
    fscan_file_chunk * bmpChp = &packp->filesStp->bitmap_chunk;
    void * headerAndBmp = NULL;
    void * bmpPointer = NULL;
    size_t graphicSize = 0;
    struct gfx_palette pal = {0};
    jmp_buf ** errbufpp = &packp->filesStp->error_jmp_buf;

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp,
        fprintf(stderr, "_fscan_prep_obj_gfx_and_exec_cb fread error\n");
        if(headerAndBmp) free(headerAndBmp);
        if(bmpPointer) free(bmpPointer);
    );

    graphicSize = fscan_read_header_and_bitmaps_alloc(mainChp, bmpChp, &headerAndBmp,
                                        &bmpPointer, packp->ext_bmp_offsetsp->v,
                                        packp->ext_bmp_offsetsp->size, &packp->filesStp->ext_bmp_index,
                                        *errbufpp, packp->bmp_headers_binds_mapp);
    if(!graphicSize) return 1;

    // palette parse
    u32 paletteOffset = fscan_read_infile_ptr(mainChp->ptrs_fp, mainChp->offset, *errbufpp);

    fseek(mainChp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->data_fp, &pal);

    // callback call
    packp->cb(packp->pass2cb, bmpPointer, headerAndBmp, &pal, iterVecp);

    //cleanup
    free(headerAndBmp);

    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

void fscan_obj_gfx_scan(struct fscan_files * filesStp, void *pass2cb,
                        void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                                const struct gfx_palette *palette, gexdev_u32vec * itervecp))
{
    gexdev_ptr_map bmp_headers_binds_map = {0};
    struct obj_gfx_scan_pack scan_pack = {filesStp, pass2cb, cb, &bmp_headers_binds_map,&filesStp->ext_bmp_offsets, 0};
    gexdev_ptr_map_init(&bmp_headers_binds_map, filesStp->main_chunk.size, fscan_cb_bmp_header_binds_compute_index);

    jmp_buf ** errbufpp = &filesStp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, 
        gexdev_ptr_map_close_all(&bmp_headers_binds_map);
        gexdev_u32vec_close(&filesStp->ext_bmp_offsets);
    )

    fscan_follow_pattern_recur(&filesStp->bitmap_chunk, "e[G{[C;]};3]", &filesStp->ext_bmp_offsets, fscan_cb_flwpat_push_offset, errbufpp);

    // TODO: DOCS OF THE PATTERN
    fscan_follow_pattern_recur(&filesStp->main_chunk, "e+0x20gg [G{ g [G{ [G{ +20 [G{ [G{ +8c };]  };2]  };] };] }+4;]",
                               &scan_pack, _fscan_prep_obj_gfx_and_exec_cb, errbufpp);

    filesStp->used_fchunks_arr[2] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    FSCAN_ERRBUF_REVERT(errbufpp);
}