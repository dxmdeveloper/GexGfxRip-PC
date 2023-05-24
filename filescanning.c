#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "filescanning.h"
#include "basicdefs.h"
#include "gfx.h"
#include "binary_parse.h"

/// NOTICE: Setting a file position beyond end of the file is UB, but most platforms handle it lol.

// mkdir / stat
#ifdef _WIN32
    #include <direct.h>
#else //POSIX
    #include <sys/stat.h>
#endif

/// TODO: insert these to the functions
/* ERRORS: -1 - failed to open a file, -2 - file is too small, -3 read error, 
   TYPES: 0 - loaded standard level file, 1 - loaded standalone gfx file
   BIT FLAGS: 2 - level file does not contain valid tiles chunk, 4 - level file does not contain valid Gfx chunk. */
enum fsmod_level_type_enum {
    FSMOD_LEVEL_TYPE_FOPEN_ERROR    = -1,
    FSMOD_LEVEL_TYPE_FILE_TOO_SMALL = -2,
    FSMOD_LEVEL_TYPE_FREAD_ERROR    = -3,
    FSMOD_LEVEL_TYPE_STANDARD       = 0,
    FSMOD_LEVEL_TYPE_GFX_ONLY       = 1,
    FSMOD_LEVEL_FLAG_NO_TILES  = 1 << 1,
    FSMOD_LEVEL_FLAG_NO_GFX    = 1 << 2,
};

// STATIC DECLARATIONS:
struct fsmod_files {
    FILE * tilesDataFp;
    FILE * tilesPtrsFp;
    FILE * gfxDataFp;
    FILE * gfxPtrsFp;
    u32 tilesChunkSize;
    u32 gfxChunkSize;
    u32 tilesChunkOffset;
    u32 gfxChunkOffset;
    u32 gfxChunkEp;
    u32 tilesChunkEp;
};

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset);
static u32 fsmod_offsetToInfilePtr(uptr fileOffset, uptr startOffset);
//static void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd);

/** @brief initializes fsmod_files structure. Opens one file in read mode multiple times and sets it at start position.
    @return enum fsmod_level_type with bit flags */
static int fsmod_files_init(struct fsmod_files * filesStp, const char filename[]);
static void fsmod_files_close(struct fsmod_files * filesStp);

/** @param nextOffset value returned by previous call of this function or 0 for the first call
    @return offsets to infile_ptr for next tiles set. First for tiles chunk, second for gfx chunk.
            first will be 0 if last set was just read.
            last will be ~0 there's no tile sets or error occured during file read. ferror & feof should be called */
static u32pair fsmod_head_to_tiles_records(struct fsmod_files * filesStp, u32pair nextOffsets);


// Extern functions definitions:
// Main module function

void fsmod_scan4Gfx(char filename[], scan_foundCallback_t foundCallback){
    struct fsmod_files filesSt = {0};
    int fileType = 0;
    jmp_buf errJmpBuf = {0};

    // File reading error jump location
    if(setjmp(errJmpBuf)){
        fprintf(stderr, "Error occured while reading file %s. The file may be corrupted\n", filename);
        fsmod_files_close(&filesSt);
        return;
    }
    // --------------------------------

    switch(fileType = fsmod_files_init(&filesSt, filename)){
        case FSMOD_LEVEL_TYPE_STANDARD: {
            if(!(fileType & FSMOD_LEVEL_FLAG_NO_GFX)){
                u32 gfxEp = 0;
                if(!fread_LE_U32(&gfxEp, 1, filesSt.gfxPtrsFp)) longjmp(errJmpBuf, 1);
                gfxEp = (u32)fsmod_infilePtrToOffset(gfxEp, filesSt.gfxChunkOffset);
                fseek(filesSt.gfxPtrsFp, gfxEp, SEEK_SET);

                if(!(fileType & FSMOD_LEVEL_FLAG_NO_TILES)){
                    // #---- Scan for tiles ----#
                    // Head to the tiles entry point
                    u32 tilesEp = 0;
                    if(!fread_LE_U32(&tilesEp, 1, filesSt.tilesPtrsFp)) longjmp(errJmpBuf, 1);
                    tilesEp = (u32)fsmod_infilePtrToOffset(tilesEp, filesSt.tilesChunkOffset);
                    
                    // 
                }
                 /*
                * TODO: GFX Chunk scan here
                */

            }
            /*
             * TODO: Intro graphics scan here
            */
        } break;
        case FSMOD_LEVEL_TYPE_GFX_ONLY: break;
        case -1: fprintf(stderr, "Failed to open file %s", filename);
        case -2: fsmod_files_close(&filesSt); return;
    }
    fsmod_files_close(&filesSt);
    #undef onFileReadError
}

// STATIC DEFINITIONS:

//  --- part of fsmod_files_init ---
/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static inline int _fsmod_files_init_open_and_set(const char filename[], FILE * mainFp, size_t fileSize, FILE * ptrsFp, FILE * dataFp, u32 * chunkSizep, u32 * chunkOffsetp, u32 * chunkEpp){
    fread_LE_U32(chunkSizep, 1, mainFp);
    fread_LE_U32(chunkOffsetp, 1, mainFp);
    if((*chunkOffsetp && *chunkSizep > 32 && *chunkOffsetp + *chunkSizep <= fileSize)) return 1;
    if(!(dataFp = fopen(filename, "rb")) 
    || !(ptrsFp = fopen(filename, "rb"))){
        return -1;
    }
    // non-ptr int arithmetics below.  Setting initial stream positions & Entry point offset
    u32 epOffset = *chunkOffsetp + *chunkSizep / 2048 + 4; //< entry point address for ptrs lookup
    fseek(dataFp, *chunkOffsetp, SEEK_SET); 
    fseek(ptrsFp, epOffset, SEEK_SET);
    fread_LE_U32(chunkEpp, 1, ptrsFp);
    *chunkEpp = (u32) fsmod_infilePtrToOffset(*chunkEpp, *chunkOffsetp);

    return 0;
}
//
static int fsmod_files_init(struct fsmod_files * filesStp, const char filename[]){
    FILE * fp = NULL;
    u32 fileChunksCount = 0;
    size_t fileSize = 0;
    int retVal = 0;

    fp = fopen(filename, "rb");
    if(fp == NULL) return -1;

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);

    if(fileSize < FILE_MIN_SIZE) return -2;
    //read first value
    rewind(fp);
    if(!fread_LE_U32(&fileChunksCount, 1, fp)) return -3;

    // Check file type
    if(fileChunksCount >= 5 && fileChunksCount <= 32){
        //FILE TYPE: STANDARD LEVEL

        // Tiles chunk setup
        fseek(fp, 0x28, SEEK_SET); 
        switch(_fsmod_files_init_open_and_set(filename, fp, fileSize,
                                          filesStp->tilesPtrsFp, filesStp->tilesDataFp,&filesStp->tilesChunkSize,
                                          &filesStp->tilesChunkOffset,&filesStp->tilesChunkEp)) {
            case -1: fclose(fp); return -1; // failed to open the file again
            case 1: retVal |= 2; break; // invalid / non-exsiting chunk
        }
        // GFX chunk setup
        fseek(fp, 0x18, SEEK_CUR);
        switch(_fsmod_files_init_open_and_set(filename, fp, fileSize,
                                          filesStp->gfxPtrsFp, filesStp->gfxDataFp,&filesStp->gfxChunkSize,
                                    &filesStp->gfxChunkOffset, &filesStp->gfxChunkEp)) {
            case -1: fclose(fp); return -1; // failed to open the file again
            case 1: retVal |= 2; break; // invalid / non-exsiting chunk
        }
    }
    else {
        // FILE TYPE: standalone gfx file
        // TODO: more standalone files detection
        retVal = 1;
    }

    if(fp != NULL) fclose(fp);
    return retVal;
}


static void fsmod_files_close(struct fsmod_files * filesStp){
    if(filesStp->gfxDataFp) {fclose(filesStp->gfxDataFp); filesStp->gfxDataFp = NULL;}
    if(filesStp->gfxPtrsFp) {fclose(filesStp->gfxPtrsFp); filesStp->gfxPtrsFp = NULL;}
    if(filesStp->tilesDataFp) {fclose(filesStp->tilesDataFp); filesStp->tilesDataFp = NULL;}
    if(filesStp->tilesPtrsFp) {fclose(filesStp->tilesPtrsFp); filesStp->tilesPtrsFp = NULL;}
}

static u32pair fsmod_head_to_tiles_records(struct fsmod_files * filesStp, u32pair nextOffset){
    u32 u32val;
    if(!nextOffset.first){
        // GFX CHUNK FILE POINTER SETUP
        fseek(filesStp->gfxPtrsFp, filesStp->gfxChunkEp + 0x21, SEEK_SET);
        if(!fread_LE_U32(&u32val, 1, filesStp->gfxPtrsFp)) return (u32pair){0, ~0};
        fseek(filesStp->gfxPtrsFp, fsmod_infilePtrToOffset(u32val, filesStp->gfxChunkOffset), SEEK_SET);
        if(!fread_LE_U32(&nextOffset.second, 1, filesStp->gfxPtrsFp)) return (u32pair){0, ~0};
        // TILES CHUNK FILE POINTER SETUP
        fseek(filesStp->tilesPtrsFp, filesStp->tilesChunkEp, SEEK_SET);
        if(!fread_LE_U32(&nextOffset.first, 1, filesStp->tilesPtrsFp)) return (u32pair){0, ~0};
    }
    // TODO: finish it!
}
static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset){
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 fsmod_offsetToInfilePtr(uptr offset, uptr startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}




