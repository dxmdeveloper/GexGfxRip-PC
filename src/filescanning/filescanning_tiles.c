#include "filescanning_tiles.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"
#include "../essentials/ptr_map.h"
#include "../helpers/basicdefs.h"

//  -------------- STATIC DECLARATIONS --------------

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedValue"
struct tile_scan_cb_pack {
    struct fscan_files * filesStp;
    void * pass2cb;
    void (*dest_cb)(void*, const void*, const void*, const struct gfx_palette*, u16, u16);
    gexdev_ptr_map * bmp_headers_binds_map;
    const gexdev_u32vec * tileBmpsOffsetsVecp; // array of 2 vectors
    
    uint bmp_index[32];
};

/** @brief made as callback for gexdev_ptr_map to shrink size of tile header pointers table */
static u32 fscan_cb_tile_header_binds_compute_index(const void* key);

/** @brief specific use function. Use as fscan_follow_pattern_recur's callback while scanning for tiles. 
  * @param clientp struct tile_scan_cb_pack */
static int fscan_prep_tile_gfx_data_and_exec_cb(fscan_file_chunk fChunkp[static 1], gexdev_u32vec * iterVec, u32 * ivars, void * clientp);

// ---------------- FUNC DEFINITIONS ----------------


void fscan_tiles_scan(struct fscan_files * filesStp, void *pass2cb,
                      void cb(void * clientp, const void *headerAndOpMap, const void *bitmap,
                              const struct gfx_palette *palette, uint16_t tileGfxId, uint16_t tileAnimFrameI))
{
    struct tile_scan_cb_pack cbPack = {filesStp, pass2cb, cb};
    gexdev_ptr_map bmp_headers_binds_map = {0};
    gexdev_u32vec tileBmpsOffsetsVec[2] = {0};
    gexdev_ptr_map_init(&bmp_headers_binds_map, filesStp->main_chunk.size / 32, fscan_cb_tile_header_binds_compute_index);
    gexdev_u32vec_init_capcity(&tileBmpsOffsetsVec[0], 16);
    gexdev_u32vec_init_capcity(&tileBmpsOffsetsVec[1], 256);

    if(!bmp_headers_binds_map.mem_regions) exit(0xbeef);
    if(!tileBmpsOffsetsVec[0].v || !tileBmpsOffsetsVec[1].v) exit(0xbeef);

    cbPack.bmp_headers_binds_map = &bmp_headers_binds_map;
    cbPack.tileBmpsOffsetsVecp = tileBmpsOffsetsVec;

    // ---------------------- error handling ----------------------
    jmp_buf ** errbufpp = &filesStp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, 
        gexdev_u32vec_close(&tileBmpsOffsetsVec[0]);
        gexdev_u32vec_close(&tileBmpsOffsetsVec[1]);
        gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    )
    // -----------------------------------------------------------


    fscan_follow_pattern_recur(&filesStp->tile_chunk, "e[G{[C;]};]", &tileBmpsOffsetsVec,
                             fscan_cb_read_offset_to_vec_2lvls, errbufpp);

    fscan_follow_pattern_recur(&filesStp->main_chunk, "e+0x28g[G{+4[r($0,1,U32) C ;]};]", &cbPack,
                             fscan_prep_tile_gfx_data_and_exec_cb, errbufpp);

    // anim tiles                         
    fscan_follow_pattern_recur(&filesStp->main_chunk, 
    "e+0x28g[G{       " /* go to tile gfx blocks                                               */
    "   g                     " /* follow first pointer in a tile gfx block                            */ 
    "   [                     " /* open null-terminated do while loop and reset counter                */
    "       +4 r($0,1,u16) -6 " /* move file cursor by 4, read tile gfx id to first internal var, back */
    "       G{[C;]}           " /* follow pointer and call callback until it returns non-zero value    */
    "       +16               " /* go to next animation                                                */
    "   ;]                    " 
    "};]                      ",
    &cbPack,fscan_prep_tile_gfx_data_and_exec_cb, errbufpp);

    filesStp->used_fchunks_arr[1] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    gexdev_u32vec_close(&tileBmpsOffsetsVec[0]);
    gexdev_u32vec_close(&tileBmpsOffsetsVec[1]);
    FSCAN_ERRBUF_REVERT(errbufpp);
}

static int fscan_prep_tile_gfx_data_and_exec_cb(fscan_file_chunk * fChunkp, gexdev_u32vec * iterVecp, u32 * ivars, void * clientp){
    struct tile_scan_cb_pack * packp = clientp;
    void * headerAndBmp = NULL;
    void * bmpPointer = NULL;
    size_t graphicSize = 0;
    struct gfx_palette pal = {0};
    fscan_file_chunk * mainChp = &packp->filesStp->main_chunk;
    fscan_file_chunk * tileChp = &packp->filesStp->tile_chunk;
    jmp_buf ** errbufpp = &packp->filesStp->error_jmp_buf;
    u32 tileGfxID = 0;
    u16 tileAnimFrameI = 0;
    uint blockIndex = iterVecp->v[0];

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp,
        fprintf(stderr, "fscan_prep_tile_gfx_data_and_exec_cb fread error\n");
        if(headerAndBmp) free(headerAndBmp);
    )

    tileGfxID = ivars[0];
    if(tileGfxID == 0xFFFFFFFF) {FSCAN_ERRBUF_REVERT(errbufpp); return 0;} // end of tile list

    if(iterVecp->size > 2){
        tileAnimFrameI = iterVecp->v[2] + 1;
    }

    if(!fscan_read_infile_ptr(mainChp->ptrs_fp, mainChp->offset, *errbufpp)){
        FSCAN_ERRBUF_REVERT(errbufpp);
        return 0;
    }
    fseek(mainChp->ptrs_fp, -4, SEEK_CUR);

    graphicSize = fscan_read_header_and_bitmaps_alloc(mainChp, tileChp, &headerAndBmp, &bmpPointer,
                                                      &packp->tileBmpsOffsetsVecp[1].v[packp->tileBmpsOffsetsVecp[0].v[blockIndex]],
                                                      packp->tileBmpsOffsetsVecp[1].size, &packp->bmp_index[blockIndex],
                                                      *errbufpp, packp->bmp_headers_binds_map, true);
    if(!graphicSize) {FSCAN_ERRBUF_REVERT(errbufpp); return 1;}

    // palette read
    u32 paletteOffset = fscan_read_infile_ptr(mainChp->ptrs_fp, mainChp->offset, *errbufpp);
    fseek(mainChp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->data_fp, &pal);

    // CALLING ONFOUND CALLBACK
    packp->dest_cb(packp->pass2cb, headerAndBmp, bmpPointer, &pal, (u16)tileGfxID, tileAnimFrameI);

    // omit 4 bytes (some flags, but only semi transparency affects tile graphics as far as I know)
    fseek(mainChp->ptrs_fp, 4, SEEK_CUR);

    // cleanup
    free(headerAndBmp);
    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

static u32 fscan_cb_tile_header_binds_compute_index(const void* key){
    const u32 * u32key = key;
    return *u32key / 32;
}
