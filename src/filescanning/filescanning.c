#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "fseeking_helper.h"
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"
#include "../essentials/ptr_map.h"

// ___________________________________________________ STRUCTURES ___________________________________________________

struct tile_scan_cb_pack {
    struct fsmod_files * filesStp;
    void * pass2cb;
    void (*dest_cb)(void*, const void*, const void*, const struct gfx_palette*, u16, u16);
    gexdev_ptr_map * bmp_headers_binds_map;
    const gexdev_u32vec * tileBmpsOffsetsVecp; // array of 2 vectors
    
    size_t bmp_index[2];
};

// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset);
static u32 fsmod_offsetToInfilePtr(uptr fileOffset, uptr startOffset);
//static void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd);

/** @brief checks file pointers for errors and eofs. if at least one has an error or eof flag jumps to error_jmp_buf
    @param mode 0 - check all, 1 - check only ptrsFps, 2 - check only dataFps */
static void fsmod_files_check_errors_and_eofs(struct fsmod_files filesStp[static 1], int mode);

/** @brief specific use function. Use as fsmod_follow_pattern_recur's callback while scanning for tiles. 
  * @param clientp struct tile_scan_cb_pack */
static int fsmod_prep_tile_gfx_data_and_exec_cb(fsmod_file_chunk fChunkp[static 1], gexdev_u32vec * iterVec, void * clientp);

/** @brief made to be used with fsmod_follow_pattern_recur.
  * @param clientp gexdev_u32vec vec[2]. First for pointing block of tile bitmaps start indexes in second vector.
  * The second vector keeps offsets of tile bitmaps */
static int fsmod_cb_read_offset_to_vec_2lvls(fsmod_file_chunk * chunkp, gexdev_u32vec * iter, void * clientp);

/** @brief made as callback for gexdev_ptr_map to shrink size of tile header pointers table */
static u32 fsmod_cb_tile_header_binds_compute_index(const void* key);

// part of fsmod_init
static inline int _fsmod_files_init_open_and_set(const char filename[], FILE * generalFp, size_t fileSize, fsmod_file_chunk fchunk[1]);


// ___________________________________________________ FUNCTION DEFINITIONS ___________________________________________________

// ------------------- Extern functions definitions: -------------------


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
    FSMOD_ERRBUF_EXTEND(filesStp->error_jmp_buf, 
        gexdev_u32vec_close(&tileBmpsOffsetsVec[0]);
        gexdev_u32vec_close(&tileBmpsOffsetsVec[1]);
        gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    )
    // -----------------------------------------------------------


    fsmod_follow_pattern_recur(&filesStp->tilesChunk, "e[G{C};]", &tileBmpsOffsetsVec,
                             fsmod_cb_read_offset_to_vec_2lvls, &filesStp->error_jmp_buf);

    fsmod_follow_pattern_recur(&filesStp->mainChunk, "e+0x28g[G{C};]", &cbPack,
                             fsmod_prep_tile_gfx_data_and_exec_cb, &filesStp->error_jmp_buf);
    
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    gexdev_u32vec_close(&tileBmpsOffsetsVec[0]);
    gexdev_u32vec_close(&tileBmpsOffsetsVec[1]);
    FSMOD_ERRBUF_REVERT(filesStp->error_jmp_buf);
}

//  --- part of fsmod_files_init ---
/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static int _fsmod_files_init_open_and_set(const char filename[], FILE * generalFp, size_t fileSize, fsmod_file_chunk fchunk[1]){
    fread_LE_U32( (u32*)&fchunk->size, 1, generalFp);
    fread_LE_U32(&fchunk->offset, 1, generalFp);
    if(!(fchunk->offset && fchunk->size > 32 && fchunk->offset + fchunk->size <= fileSize)) return 1;
    if(!(fchunk->ptrsFp = fopen(filename, "rb")) 
    || !(fchunk->dataFp = fopen(filename, "rb"))){
        return -1;
    }
    // non-ptr int arithmetics below.  Setting initial stream positions & Entry point offset
    u32 epOffset = fchunk->offset + fchunk->size / 2048 + 4; //< entry point address for ptrs lookup
    fseek(fchunk->dataFp, fchunk->offset, SEEK_SET); 
    fseek(fchunk->ptrsFp, epOffset, SEEK_SET);
    fread_LE_U32(&fchunk->ep, 1, fchunk->ptrsFp);
    fchunk->ep = (u32) fsmod_infilePtrToOffset(fchunk->ep, fchunk->offset);

    return 0;
}

int fsmod_files_init(struct fsmod_files * filesStp, const char filename[]){
    FILE * fp = NULL;
    u32 fileChunksCount = 0;
    size_t fileSize = 0;
    int retVal = 0;

    fp = fopen(filename, "rb");
    if(fp == NULL) return FSMOD_LEVEL_TYPE_FOPEN_ERROR;

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);

    if(fileSize < FILE_MIN_SIZE) return FSMOD_LEVEL_TYPE_FILE_TOO_SMALL;
    //read first value
    rewind(fp);
    if(!fread_LE_U32(&fileChunksCount, 1, fp)) return -3;

    // Check file type
    if(fileChunksCount >= 5 && fileChunksCount <= 32){
        //FILE TYPE: STANDARD LEVEL

        // Tiles chunk setup
        fseek(fp, 0x28, SEEK_SET); 
        switch(_fsmod_files_init_open_and_set(filename, fp, fileSize, &filesStp->tilesChunk)) {
            case -1: fclose(fp); return FSMOD_LEVEL_TYPE_FOPEN_ERROR;
            case 1: retVal |= 2; break; // invalid / non-exsiting chunk
        }
        // GFX chunk setup
        fseek(fp, 0x18, SEEK_CUR);
        switch(_fsmod_files_init_open_and_set(filename, fp, fileSize, &filesStp->mainChunk)) {
            case -1: fclose(fp); return FSMOD_LEVEL_TYPE_FOPEN_ERROR;
            case 1: retVal |= 2; break; // invalid / non-exsiting chunk
        }
    }
    else {
        // FILE TYPE: standalone gfx file
        // TODO: more special files detection
        retVal = 1;
    }

    if(fp != NULL) fclose(fp);
    return retVal;
}


void fsmod_files_close(struct fsmod_files * filesStp){
    if(filesStp->mainChunk.ptrsFp) {fclose(filesStp->mainChunk.ptrsFp); filesStp->mainChunk.ptrsFp = NULL;}
    if(filesStp->mainChunk.dataFp) {fclose(filesStp->mainChunk.dataFp); filesStp->mainChunk.dataFp = NULL;}
    if(filesStp->tilesChunk.ptrsFp) {fclose(filesStp->tilesChunk.ptrsFp); filesStp->tilesChunk.ptrsFp = NULL;}
    if(filesStp->tilesChunk.dataFp) {fclose(filesStp->tilesChunk.dataFp); filesStp->tilesChunk.dataFp = NULL;}
}


u32 fsmod_read_infile_ptr(FILE * fp, u32 chunkOffset, jmp_buf *error_jmp_buf){
    u32 val = 0;
    if(!fread_LE_U32(&val, 1, fp) && error_jmp_buf)
        longjmp(*error_jmp_buf, FSMOD_READ_ERROR_FREAD);

    return (u32)fsmod_infilePtrToOffset(val, chunkOffset);
}

size_t fsmod_fread(void *dest, size_t size, size_t n, FILE * fp, jmp_buf *error_jmp_buf){
    size_t retval = fread(dest, size, n, fp);
    if(retval < n && error_jmp_buf)
        longjmp(*error_jmp_buf, FSMOD_READ_ERROR_FREAD);
    return retval;
}


// ------------------- Static functions definitions: -------------------

static int fsmod_cb_read_offset_to_vec_2lvls(fsmod_file_chunk * chp, gexdev_u32vec* iter, void * clientp){
    gexdev_u32vec * vec_arr = clientp; //[2]
    u32 offset = fsmod_read_infile_ptr(chp->ptrsFp, chp->offset, NULL);
    if(!offset) return 0;

    while(vec_arr[0].size <= iter->v[0]){
        gexdev_u32vec_push_back(&vec_arr[0], vec_arr[1].size);
    }

    gexdev_u32vec_push_back(&vec_arr[1], offset);
    return 1;
}

static int fsmod_prep_tile_gfx_data_and_exec_cb(fsmod_file_chunk * fChunkp, gexdev_u32vec * iterVecp, void * clientp){
    struct tile_scan_cb_pack * packp = clientp;
    void * header = NULL;
    void * bitmap = NULL;
    struct gfx_palette pal = {0};
    fsmod_file_chunk * mainChp = &packp->filesStp->mainChunk;
    fsmod_file_chunk * tileChp = &packp->filesStp->tilesChunk;
    jmp_buf ** errbufpp = &packp->filesStp->error_jmp_buf;

    // error handling
    FSMOD_ERRBUF_EXTEND(*errbufpp,
        fprintf(stderr, "fsmod_prep_tile_gfx_data_and_exec_cb fread error\n");
        if(header) free(header);
    );

    // check if the current iteration is in new tile gfx block
    if(packp->bmp_index[0] != iterVecp->v[0]){
        packp->bmp_index[0] = iterVecp->v[0];
        packp->bmp_index[1] = 0;
    }

    fseek(mainChp->ptrsFp, 4, SEEK_CUR);

    // tile graphic id read
    u32 tileGfxID = 0;
    fread_LE_U32(&tileGfxID, 1, mainChp->ptrsFp);
    if(tileGfxID == 0xFFFFFFFF) {FSMOD_ERRBUF_REVERT(*errbufpp); return 0;} // end of tile list

    // gfx header read
    u32 headerOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, *errbufpp);
    u32 paletteOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, *errbufpp);

    if(!headerOffset){FSMOD_ERRBUF_REVERT(*errbufpp); return 1;}
    fseek(mainChp->dataFp, headerOffset, SEEK_SET);

    if(!gex_gfxHeadersFToAOB(mainChp->dataFp, &header)) /* <- mem allocated */{
        // HEADER WITHOUT DIMENSIONS = SKIP BITMAP
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
        if(packp->tileBmpsOffsetsVecp[0].size <= packp->bmp_index[0])
            longjmp(**errbufpp, FSMOD_ERROR_INDEX_OUT_OF_RANGE);
         
        offsetIndex = packp->tileBmpsOffsetsVecp[0].v[packp->bmp_index[0]] + packp->bmp_index[1];
        if(packp->tileBmpsOffsetsVecp[1].size <= offsetIndex)
            longjmp(**errbufpp, FSMOD_ERROR_INDEX_OUT_OF_RANGE);

        // new bitmap read
        fseek(tileChp->dataFp, packp->tileBmpsOffsetsVecp[1].v[offsetIndex] + 4 /* bmp size skip */, SEEK_SET);
        packp->bmp_index[1]++;
        
        if(!(bitmap = malloc(required_size))) exit(0xbeef); // freed in gexdev_ptr_map_close_all
        fsmod_fread(bitmap, 1, required_size, tileChp->dataFp, *errbufpp); 
        gexdev_ptr_map_set(packp->bmp_headers_binds_map, &rel_header_offset, bitmap);
    }

    // palette read
    fseek(mainChp->dataFp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->dataFp, &pal);

    // CALLING ONFOUND CALLBACK
    packp->dest_cb(packp->pass2cb, bitmap, header, &pal, tileGfxID, 0);

    // cleanup
    if(header) free(header);
    FSMOD_ERRBUF_REVERT(*errbufpp);
    return 1;
}

/*
static void fsmod_files_check_errors_and_eofs(struct fsmod_files * filesStp, int mode){
    if(!filesStp->error_jmp_buf) return;
    switch(mode){
        case 0:
        case 1:
            if(feof(filesStp->tilesPtrsFp) || feof(filesStp->gfxPtrsFp))
                longjmp(*filesStp->error_jmp_buf, FSMOD_READ_ERROR_UNEXPECTED_EOF);
            if(ferror(filesStp->tilesPtrsFp) || ferror(filesStp->gfxPtrsFp)) 
                longjmp(*filesStp->error_jmp_buf, FSMOD_READ_ERROR_FERROR);
            if(mode) return;
        case 2:
            if(feof(filesStp->tilesDataFp) || feof(filesStp->gfxDataFp))
                longjmp(*filesStp->error_jmp_buf, FSMOD_READ_ERROR_UNEXPECTED_EOF);
            if(ferror(filesStp->tilesDataFp) || ferror(filesStp->gfxDataFp)) 
                longjmp(*filesStp->error_jmp_buf, FSMOD_READ_ERROR_FERROR);
            break;
    }
}
*/

static u32 fsmod_cb_tile_header_binds_compute_index(const void* key){
    const u32 * u32keyp = key;
    return *u32keyp / 32;
}

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset){
    if(infile_ptr == 0) return 0;
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 fsmod_offsetToInfilePtr(uptr offset, uptr startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}
