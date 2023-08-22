#include "filescanning_obj_gfx_and_bg.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../essentials/ptr_map.h"
#include "../helpers/binary_parse.h"

// TODO: DOCS OF THE PATTERNS
#define FSCAN_OBJ_GFX_FLW_PATTERN "e+0x20gg [G{ g [G{ [G{ +24 g [G{ +8c };]   };] };] }+4;]"
#define FSCAN_BACKGROUND_FLW_PATTERN "eg+24[G{ +48 [G{ +4 [g;4]+24g [G{ +8c };] };] };]"

struct obj_gfx_scan_pack {
    struct fscan_files * filesStp;
    void * pass2cb;
    void (*cb)(void * clientp, const void *bitmap, const void *headerAndOpMap,
               const struct gfx_palette *palette, u32 iters[3]);
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

// TODO: REMOVE?
static int fscan_cb_ext_bmp_index_count(fscan_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 *iv, void * clientp){
    struct obj_gfx_scan_pack * packp = clientp;
    u32 headerOffset = fscan_read_infile_ptr(fChunk->ptrs_fp, fChunk->offset, NULL);
    void * mappedBmp = NULL;
    u8 gfxType = 0;
    void * headerData = NULL;
    fseek(fChunk->ptrs_fp, headerOffset+19, SEEK_SET);
    fread(&gfxType, 1, 1,fChunk->ptrs_fp);
    if((gfxType & 0xC0) != 0xC0) return 1;

    fseek(fChunk->data_fp, headerOffset, SEEK_SET);
    if(!gfx_read_headers_alloc_aob(fChunk->data_fp, &headerData)){
        packp->filesStp->ext_bmp_index++;
        return 1;
    }

    u32 relOffset = headerOffset - fChunk->offset;
    if((mappedBmp = gexdev_ptr_map_get(packp->bmp_headers_binds_mapp, &relOffset))){
        if(headerData) free(headerData);
        return 1;
    }

    for(void * gch = headerData+20; *(u32*)gch; gch += 8) {
        packp->filesStp->ext_bmp_index++;
    }

    gexdev_ptr_map_set(packp->bmp_headers_binds_mapp, &relOffset, malloc(1));
    if(headerData) free(headerData);
    return 1;
}

static int fscan_cb_prep_obj_gfx_and_exec_cb(fscan_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 *iv, void * clientp){
    struct obj_gfx_scan_pack * packp = clientp;
    fscan_file_chunk * mainChp = fChunk;
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
    );

    graphicSize = fscan_read_header_and_bitmaps_alloc(mainChp, bmpChp, &headerAndBmp,
                                        &bmpPointer, packp->ext_bmp_offsetsp->v,
                                        packp->ext_bmp_offsetsp->size, &packp->ext_bmp_index,
                                        *errbufpp, packp->bmp_headers_binds_mapp);
    if(!graphicSize) {FSCAN_ERRBUF_REVERT(errbufpp); return 1;}

    // palette parse
    u32 paletteOffset = fscan_read_infile_ptr(mainChp->ptrs_fp, mainChp->offset, *errbufpp);

    fseek(mainChp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->data_fp, &pal);

    // callback call
    packp->cb(packp->pass2cb, headerAndBmp, bmpPointer, &pal, iterVecp->v);

    //cleanup
    free(headerAndBmp);

    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

inline static void fscan_scan_chunk_for_obj_gfx(struct fscan_files * filesStp, fscan_file_chunk * fChp,
                                                bool dontPrepGfxData, char * flwPattern, void *pass2cb,
                                                void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *))
{
    gexdev_ptr_map bmp_headers_binds_map = {0};
    struct obj_gfx_scan_pack scan_pack = {filesStp, pass2cb, cb, &bmp_headers_binds_map,&filesStp->ext_bmp_offsets, 0};
    gexdev_ptr_map_init(&bmp_headers_binds_map, fChp->size, fscan_cb_bmp_header_binds_compute_index);

    jmp_buf ** errbufpp = &filesStp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp,
            gexdev_ptr_map_close_all(&bmp_headers_binds_map);
            gexdev_u32vec_close(&filesStp->ext_bmp_offsets);
    )

    if(!filesStp->used_fchunks_arr[2])
        fscan_follow_pattern_recur(&filesStp->bitmap_chunk, "e[G{[C;]};3]", &filesStp->ext_bmp_offsets, fscan_cb_flwpat_push_offset, errbufpp);

    fscan_follow_pattern_recur(fChp, flwPattern,
                               &scan_pack,
                               (dontPrepGfxData ? fscan_cb_ext_bmp_index_count : fscan_cb_prep_obj_gfx_and_exec_cb),
                               errbufpp);

    filesStp->used_fchunks_arr[2] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    FSCAN_ERRBUF_REVERT(errbufpp);
}

void fscan_obj_gfx_scan(struct fscan_files * filesStp, void *pass2cb,
                        void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *))
{
    if(filesStp->used_fchunks_arr[2]) filesStp->ext_bmp_index = 0;

    fscan_scan_chunk_for_obj_gfx(filesStp, &filesStp->main_chunk, false,
                                 FSCAN_OBJ_GFX_FLW_PATTERN, pass2cb, cb);

    filesStp->used_fchunks_arr[3] = true;
}

void fscan_intro_obj_gfx_scan(struct fscan_files * filesStp, void *pass2cb,
                              void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *))
{
    if(filesStp->used_fchunks_arr[4] || filesStp->used_fchunks_arr[5]) filesStp->ext_bmp_index = 0;

    if(!filesStp->ext_bmp_index && filesStp->main_chunk.ptrs_fp){
        fscan_scan_chunk_for_obj_gfx(filesStp, &filesStp->main_chunk, true,
                                     FSCAN_OBJ_GFX_FLW_PATTERN, NULL, NULL);

        filesStp->used_fchunks_arr[3] = true;
    }

    fscan_scan_chunk_for_obj_gfx(filesStp, &filesStp->intro_chunk, false, FSCAN_OBJ_GFX_FLW_PATTERN, pass2cb, cb);
    filesStp->used_fchunks_arr[4] = true;
}



void _old_fscan_background_scan(struct fscan_files *filesStp, void *pass2cb,
                                void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *))
{
    if(filesStp->used_fchunks_arr[5]){
        filesStp->ext_bmp_index = 0;
        filesStp->used_fchunks_arr[3] = false;
        filesStp->used_fchunks_arr[4] = false;
    }
    // Scan main and intro chunks before if not scanned yet
    if(!filesStp->used_fchunks_arr[3] && filesStp->main_chunk.ptrs_fp){
        fscan_scan_chunk_for_obj_gfx(filesStp, &filesStp->main_chunk, true, FSCAN_OBJ_GFX_FLW_PATTERN, NULL, NULL);
        filesStp->used_fchunks_arr[3] = true;
    }
    if(!filesStp->used_fchunks_arr[4] && filesStp->intro_chunk.ptrs_fp){
        fscan_scan_chunk_for_obj_gfx(filesStp, &filesStp->intro_chunk, true, FSCAN_OBJ_GFX_FLW_PATTERN, NULL, NULL);
        filesStp->used_fchunks_arr[4] = true;
    }

    // Scan background file chunk
    fscan_scan_chunk_for_obj_gfx(filesStp, &filesStp->bg_chunk, false, FSCAN_BACKGROUND_FLW_PATTERN, pass2cb, cb);
    filesStp->used_fchunks_arr[5] = true;
}

void fscan_background_scan(struct fscan_files *filesStp, void *pass2cb,
                           void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *))
{
    gexdev_ptr_map bmp_headers_binds_map = {0};
    gexdev_u32vec ext_bmp_offsets = {0};
    struct obj_gfx_scan_pack scan_pack = {filesStp, pass2cb, cb, &bmp_headers_binds_map,&ext_bmp_offsets, 0};
    gexdev_ptr_map_init(&bmp_headers_binds_map, filesStp->bg_chunk.size, fscan_cb_bmp_header_binds_compute_index);
    gexdev_u32vec_init_capcity( &ext_bmp_offsets, 256);

    jmp_buf ** errbufpp = &filesStp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp,
        gexdev_ptr_map_close_all(&bmp_headers_binds_map);
        gexdev_u32vec_close(&ext_bmp_offsets);
    )

    fscan_follow_pattern_recur(&filesStp->bitmap_chunk, "e+8g [C;]", &ext_bmp_offsets, fscan_cb_flwpat_push_offset, errbufpp);

    fscan_follow_pattern_recur(&filesStp->bg_chunk, FSCAN_BACKGROUND_FLW_PATTERN,&scan_pack,  fscan_cb_prep_obj_gfx_and_exec_cb, errbufpp);

    filesStp->used_fchunks_arr[2] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    FSCAN_ERRBUF_REVERT(errbufpp);
}

