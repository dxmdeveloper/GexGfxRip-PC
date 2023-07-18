#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "fseeking_helper.h"
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"

/// NOTICE: Setting a file position beyond end of the file is UB, but most platforms handle it lol.


// ___________________________________________________ STRUCTURES ___________________________________________________

struct tile_scan_cb_pack {
    struct fsmod_files * filesStp;
    void * pass2cb;
    void (*dest_cb)(void*, const void*, const void*, const struct gfx_palette*, const char[]);
    void ** bmp_headers_binds_map;
    const gexdev_u32vec * tileBmpsOffsetsVecp;
    
    size_t cb_iteration; //?
    size_t bmp_index;
    u32 lastLvl; //?
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

/////** @brief reads tile header and bitmap into arrays, creates palette, calls callback 
////   @param filesStp file pointers must be set at the correct positions (tile graphic entries) */
////static void fsmod_prep_tile_gfx_data_and_exec_cb(struct fsmod_files * filesStp, void * pass2cb, const char suggestedName[],
////                                                void cb(void*, const void*, const void*, const struct gfx_palette*, const char[]));

/** @brief made to be used with fsmod_follow_pattern_recur.
    @param clientp gexdev_u32vec pointer */
static int fsmod_cb_read_offset_to_vec(fsmod_file_chunk * chunkp, gexdev_u32vec *  ignored, void * clientp);

// part of fsmod_init
static inline int _fsmod_files_init_open_and_set(const char filename[], FILE * generalFp, size_t fileSize, fsmod_file_chunk fchunk[1]);


// ___________________________________________________ FUNCTION DEFINITIONS ___________________________________________________

// ------------------- Extern functions definitions: -------------------

void fsmod_tiles_scan(struct fsmod_files * filesStp, void *pass2cb,
                      void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                              const struct gfx_palette *palette, const char suggestedName[]))
{
    struct tile_scan_cb_pack cbPack = {filesStp, pass2cb, cb};
    gexdev_u32vec tileBmpsOffsetsVec = {0};
    gexdev_u32vec_init_capcity(&tileBmpsOffsetsVec, 512);

    // ------------------- error handling ---------------
    jmp_buf errbuf; jmp_buf * errbufp = &errbuf; int errno = 0;
    jmp_buf * prev_errbufp = filesStp->error_jmp_buf;
    if((errno = setjmp(errbuf))){
        gexdev_u32vec_close(&tileBmpsOffsetsVec);
        if(cbPack.bmp_headers_binds_map){
            // TODO: CREATE DATA STRUCTURE FOR DICTIONARY / HASH MAP
            for(size_t i = 0; i < filesStp->mainChunk.size / 28; i++)
                if(cbPack.bmp_headers_binds_map[i]) free(cbPack.bmp_headers_binds_map[i]);
            free(cbPack.bmp_headers_binds_map);
        } 
        filesStp->error_jmp_buf = prev_errbufp;
        if(prev_errbufp) longjmp(*prev_errbufp, errno);
    }
    filesStp->error_jmp_buf = errbufp;
    // ----------------------------------------------------

    cbPack.bmp_headers_binds_map = calloc(filesStp->mainChunk.size / 28 + 1, sizeof(void*));
    cbPack.tileBmpsOffsetsVecp = &tileBmpsOffsetsVec;

    fsmod_follow_pattern_recur(&filesStp->tilesChunk, "e[G{C};]", &tileBmpsOffsetsVec, fsmod_cb_read_offset_to_vec, errbufp);

    fsmod_follow_pattern_recur(&filesStp->mainChunk, "e+0x28g[G{C};]", &cbPack,
                             fsmod_prep_tile_gfx_data_and_exec_cb, errbufp);
    
    // cleanup
    if(cbPack.bmp_headers_binds_map){
        for(size_t i = 0; i < filesStp->mainChunk.size / 28; i++)
            if(cbPack.bmp_headers_binds_map[i]) free(cbPack.bmp_headers_binds_map[i]);
        free(cbPack.bmp_headers_binds_map);
    }
    gexdev_u32vec_close(&tileBmpsOffsetsVec);
    filesStp->error_jmp_buf = prev_errbufp;
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

static int fsmod_cb_read_offset_to_vec(fsmod_file_chunk * chp, gexdev_u32vec* ignored, void * clientp){
    gexdev_u32vec * vecp = clientp;
    u32 offset = fsmod_read_infile_ptr(chp->ptrsFp, chp->offset, NULL);
    if(!offset) return 0;
    gexdev_u32vec_push_back(vecp, offset);
    return 1;
}

static int fsmod_prep_tile_gfx_data_and_exec_cb(fsmod_file_chunk * fChunkp, gexdev_u32vec * iterVecp, void * clientp){
    struct tile_scan_cb_pack * packp = clientp;
    char suggestedName[36] = "FFFF";
    void * header = NULL;
    fsmod_file_chunk * mainChp = &packp->filesStp->mainChunk;
    fsmod_file_chunk * tileChp = &packp->filesStp->tilesChunk;

    jmp_buf errbuf; jmp_buf * errbufp; int errno = 0;
    errbufp = &errbuf;
    // fread error handling
    if((errno = setjmp(errbuf))){
        if(header) free(header);
        fprintf(stderr, "fsmod_prep_tile_gfx_data_and_exec_cb fread error (recur iters: %"PRIu32"-%"PRIu32")\n",
                iterVecp->v[0], iterVecp->v[1]);
        return 0;
    }
    // - - - -- - - - - -
    
    if(packp->lastLvl != iterVecp->v[0]){
        packp->lastLvl++;
        /*
        sprintf(pattern, "e+40g+%ug", (uint)packp->lastLvl);
        if(fsmod_follow_pattern(mainChp, pattern, errbufp)){
            dbg_errlog("not found matching tile block in mainChunk");
            return;
        }
        */
    } // ?

    fseek(mainChp->ptrsFp, 4, SEEK_CUR);

    u32 tileID = 0;
    fread_LE_U32(&tileID, 1, mainChp->ptrsFp);
    if(tileID == 0xFFFFFFFF) return 0; // end of tile list

    sprintf(suggestedName, "%u-%04"PRIX16".png",(uint)packp->cb_iteration, (u16)tileID);

    u32 headerOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, errbufp);
    u32 paletteOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, errbufp);

    if(!headerOffset) return 1;
    fseek(mainChp->dataFp, headerOffset, SEEK_SET);
    
    {
        void * bitmap = NULL;
        struct gfx_palette pal = {0};
        size_t bitmap_size = 0, required_size = 0;
        u16 tileWidthAndHeight[2] = {0};

        //fread_LE_U16(tileWidthAndHeight, 2, tileChp->dataFp); // TODO: error handling
        //bitmap_size = tileWidthAndHeight[0] * 2 * tileWidthAndHeight[1]; // TODO: fix bitmap size calc

        if(!gex_gfxHeadersFToAOB(mainChp->dataFp, &header)) /* <- mem allocated */{
            fprintf(stderr,"bad header at: %lu (%zu)\n", ftell(mainChp->dataFp), packp->cb_iteration);
            if(header) free(header); header = NULL;

            //if(!packp->bmp_headers_binds_map[(headerOffset - mainChp->offset) / 28]){
                packp->bmp_index++;
            //}
            //longjmp(errbufp, FSMOD_READ_ERROR_WRONG_VALUE);
            return 1; // skip
        } 

        required_size = gfx_checkSizeOfBitmap(header);
        //! TESTS 
        //bitmap_size *= 2; // ?????

        if(/*bitmap_size >= required_size &&*/ required_size){
            // bitmap data read if not cached
            if(packp->bmp_headers_binds_map[(headerOffset - mainChp->offset) / 28])
                bitmap = packp->bmp_headers_binds_map[(headerOffset - mainChp->offset) / 28];
            else {
                if(packp->tileBmpsOffsetsVecp->size <= packp->bmp_index) return 0; //?
                fseek(tileChp->dataFp, packp->tileBmpsOffsetsVecp->v[packp->bmp_index] + 4 /* bmp size skip */, SEEK_SET);
                packp->bmp_index++;
                
                if(!(bitmap = malloc(required_size))) exit(0xbeef);
                fsmod_fread(bitmap, 1, required_size, tileChp->dataFp, errbufp); 
                packp->bmp_headers_binds_map[(headerOffset - mainChp->offset) / 28] = bitmap;
            }

            // palette read
            fseek(mainChp->dataFp, paletteOffset, SEEK_SET);
            gfx_palette_parsef(mainChp->dataFp, &pal);

            // CALLING ONFOUND CALLBACK
            packp->dest_cb(packp->pass2cb, bitmap, header, &pal, suggestedName);
        }

    }
    if(header) free(header);
    packp->cb_iteration++;
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

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset){
    if(infile_ptr == 0) return 0;
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 fsmod_offsetToInfilePtr(uptr offset, uptr startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}
