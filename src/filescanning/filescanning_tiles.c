#include "filescanning_tiles.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"
#include "../essentials/ptr_map.h"
#include "../helpers/basicdefs.h"

//  -------------- STATIC DECLARATIONS --------------

struct tile_scan_cb_pack {
    struct fsmod_files * filesStp;
    void * pass2cb;
    void (*dest_cb)(void*, const void*, const void*, const struct gfx_palette*, u16, u16);
    gexdev_ptr_map * bmp_headers_binds_map;
    const gexdev_u32vec * tileBmpsOffsetsVecp; // array of 2 vectors
    
    size_t bmp_index[32];
    u32 lastAnimGfxId;
    u32 lastAnimFrameI;
};

/** @brief made as callback for gexdev_ptr_map to shrink size of tile header pointers table */
static u32 fsmod_cb_tile_header_binds_compute_index(const void* key);

/** @brief specific use function. Use as fsmod_follow_pattern_recur's callback while scanning for tiles. 
  * @param clientp struct tile_scan_cb_pack */
static int fsmod_prep_tile_gfx_data_and_exec_cb(fsmod_file_chunk fChunkp[static 1], gexdev_u32vec * iterVec, u32 * ivars, void * clientp);

// ---------------- FUNC DEFINITIONS ----------------


void fsmod_tiles_scan(struct fsmod_files * filesStp, void *pass2cb,
                      void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                              const struct gfx_palette *palette, uint16_t tileGfxId, uint16_t tileAnimFrameI))
{
    struct tile_scan_cb_pack cbPack = {filesStp, pass2cb, cb};
    gexdev_ptr_map bmp_headers_binds_map = {0};
    gexdev_u32vec tileBmpsOffsetsVec[2] = {0};
    gexdev_ptr_map_init(&bmp_headers_binds_map, filesStp->mainChunk.size / 32, fsmod_cb_tile_header_binds_compute_index);
    gexdev_u32vec_init_capcity(&tileBmpsOffsetsVec[0], 16);
    gexdev_u32vec_init_capcity(&tileBmpsOffsetsVec[1], 256);

    if(!bmp_headers_binds_map.mem_regions) exit(0xbeef);
    if(!tileBmpsOffsetsVec[0].v || !tileBmpsOffsetsVec[1].v) exit(0xbeef);

    cbPack.bmp_headers_binds_map = &bmp_headers_binds_map;
    cbPack.tileBmpsOffsetsVecp = tileBmpsOffsetsVec;

    // ---------------------- error handling ----------------------
    FSMOD_ERRBUF_CHAIN_ADD(filesStp->error_jmp_buf, 
        gexdev_u32vec_close(&tileBmpsOffsetsVec[0]);
        gexdev_u32vec_close(&tileBmpsOffsetsVec[1]);
        gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    )
    // -----------------------------------------------------------


    fsmod_follow_pattern_recur(&filesStp->tilesChunk, "e[G{C};]", &tileBmpsOffsetsVec,
                             fsmod_cb_read_offset_to_vec_2lvls, &filesStp->error_jmp_buf);

    fsmod_follow_pattern_recur(&filesStp->mainChunk, "e+0x28g[G{+4C};]", &cbPack,
                             fsmod_prep_tile_gfx_data_and_exec_cb, &filesStp->error_jmp_buf);

    // anim tiles                         
    fsmod_follow_pattern_recur(&filesStp->mainChunk, 
    "e+0x28g[G{       " /* go to tile gfx blocks                                               */
    "   g                     " /* follow first pointer in a tile gfx block                            */ 
    "   [                     " /* open null-terminated do while loop and reset counter                */
    "       +4 r($0,1,u16) -6 " /* move file cursor by 4, read tile gfx id to first internal var, back */
    "       G{C}              " /* follow pointer and call callback until it return non-zero value     */
    "       +16               " /* go to next animation                                                */
    "   ;]                    " 
    "};]                      ",
    &cbPack,fsmod_prep_tile_gfx_data_and_exec_cb, &filesStp->error_jmp_buf);

    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    gexdev_u32vec_close(&tileBmpsOffsetsVec[0]);
    gexdev_u32vec_close(&tileBmpsOffsetsVec[1]);
    FSMOD_ERRBUF_REVERT(filesStp->error_jmp_buf);
}

static int fsmod_prep_tile_gfx_data_and_exec_cb(fsmod_file_chunk * fChunkp, gexdev_u32vec * iterVecp, u32 * ivars, void * clientp){
    struct tile_scan_cb_pack * packp = clientp;
    void * header = NULL;
    void * bitmap = NULL;
    struct gfx_palette pal = {0};
    fsmod_file_chunk * mainChp = &packp->filesStp->mainChunk;
    fsmod_file_chunk * tileChp = &packp->filesStp->tilesChunk;
    jmp_buf ** errbufpp = &packp->filesStp->error_jmp_buf;
    u32 tileGfxID = 0;
    u16 tileAnimFrameI = 0;
    uint blockIndex = iterVecp->v[0];

    // error handling
    FSMOD_ERRBUF_CHAIN_ADD(*errbufpp,
        fprintf(stderr, "fsmod_prep_tile_gfx_data_and_exec_cb fread error\n");
        if(header) free(header);
    );

    // base tile
    if(!ivars[0]){
        // tile graphic id read
        fread_LE_U32(&tileGfxID, 1, mainChp->ptrsFp);
        if(tileGfxID == 0xFFFFFFFF) {FSMOD_ERRBUF_REVERT(*errbufpp); return 0;} // end of tile list

    // tile animation frame
    } else {
        tileGfxID = ivars[0];

        if(packp->lastAnimGfxId != iterVecp->v[1]){
            packp->lastAnimGfxId = iterVecp->v[1];
            packp->lastAnimFrameI = 1;
        }
        tileAnimFrameI = packp->lastAnimFrameI++;
    }

    // gfx header read
    u32 headerOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, *errbufpp);
    u32 paletteOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, *errbufpp);

    if(!headerOffset){FSMOD_ERRBUF_REVERT(*errbufpp); return 0;}
    fseek(mainChp->dataFp, headerOffset, SEEK_SET);

    if(!gex_gfxHeadersFToAOB(mainChp->dataFp, &header)) /* <- mem allocated */{
        // HEADER WITHOUT DIMENSIONS = SKIP BITMAP
        fseek(mainChp->ptrsFp, 4, SEEK_CUR);
        if(header) free(header); header = NULL;
        packp->bmp_index[1]++;
        FSMOD_ERRBUF_REVERT(*errbufpp);
        return 1;
    }

    size_t required_size = gfx_checkSizeOfBitmap(header);

    // read next bitmap if is not already read. Otherwise don't increase bmp_index
    u32 rel_header_offset = headerOffset - mainChp->offset; //< for smaller ptr map size
    void * mapped_ptr = gexdev_ptr_map_get(packp->bmp_headers_binds_map, &rel_header_offset);
    if(mapped_ptr)
        // use bitmap that was already read and assigned to header offset
        bitmap = mapped_ptr;
    else {
        u32 offsetIndex = 0;
        if(packp->tileBmpsOffsetsVecp[0].size <= blockIndex || blockIndex >= 32)
            longjmp(**errbufpp, FSMOD_ERROR_INDEX_OUT_OF_RANGE);
         
        offsetIndex = packp->tileBmpsOffsetsVecp[0].v[iterVecp->v[0]] + packp->bmp_index[blockIndex];
        if(packp->tileBmpsOffsetsVecp[1].size <= offsetIndex)
            longjmp(**errbufpp, FSMOD_ERROR_INDEX_OUT_OF_RANGE);

        // new bitmap read
        fseek(tileChp->dataFp, packp->tileBmpsOffsetsVecp[1].v[offsetIndex] + 4 /* bmp size skip */, SEEK_SET);
        packp->bmp_index[blockIndex]++;
        
        if(!(bitmap = malloc(required_size))) exit(0xbeef); // freed in gexdev_ptr_map_close_all
        fsmod_fread(bitmap, 1, required_size, tileChp->dataFp, *errbufpp); 
        gexdev_ptr_map_set(packp->bmp_headers_binds_map, &rel_header_offset, bitmap);
    }

    // palette read
    fseek(mainChp->dataFp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->dataFp, &pal);

    // CALLING ONFOUND CALLBACK
    packp->dest_cb(packp->pass2cb, bitmap, header, &pal, (u16)tileGfxID, tileAnimFrameI);

    // omit 4 bytes (I don't know what it is)
    fseek(mainChp->ptrsFp, 4, SEEK_CUR);

    // cleanup
    if(header) free(header);
    FSMOD_ERRBUF_REVERT(*errbufpp);
    return 1;
}

static u32 fsmod_cb_tile_header_binds_compute_index(const void* key){
    const u32 * u32key = key;
    return *u32key / 32;
}