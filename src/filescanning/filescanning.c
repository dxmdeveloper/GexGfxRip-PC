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

// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset);
static u32 fsmod_offsetToInfilePtr(uptr fileOffset, uptr startOffset);

// part of fsmod_init
static inline int _fsmod_files_init_open_and_set(const char filename[], FILE * generalFp, size_t fileSize, fsmod_file_chunk fchunk[1]);

// _______________________________________________________ FUNCTION DEFINITIONS _______________________________________________________


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


int fsmod_cb_read_offset_to_vec_2lvls(fsmod_file_chunk * chp, gexdev_u32vec* iter, void * clientp){
    gexdev_u32vec * vec_arr = clientp; //[2]
    u32 offset = fsmod_read_infile_ptr(chp->ptrsFp, chp->offset, NULL);
    if(!offset) return 0;

    while(vec_arr[0].size <= iter->v[0]){
        gexdev_u32vec_push_back(&vec_arr[0], vec_arr[1].size);
    }

    gexdev_u32vec_push_back(&vec_arr[1], offset);
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
