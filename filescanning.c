#include <stdlib.h>
#include <stdio.h>
#include "filescanning.h"
#include "basicdefs.h"
#include "gfx.h"
#include "binary_parse.h"

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
};

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset);
static u32 fsmod_offsetToInfilePtr(uptr fileOffset, uptr startOffset);
//static void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd);
static void scanChunk(void *startOffset, void *endOffset, scan_foundCallback_t, char path[], u32 pathLen, uintptr_t inf_fileDataAlloc);

/** @brief initializes fsmod_files structure. Opens one file in read mode multiple times and sets it at start position.
    @return enum fsmod_level_type with bit flags */
static int fsmod_files_init(struct fsmod_files * filesStp, char filename[]);
static void fsmod_files_close(struct fsmod_files * filesStp);

// Extern functions definitions:
// Main module function

void fsmod_scan4Gfx(char filename[], scan_foundCallback_t foundCallback){
    struct fsmod_files filesSt = {0};
    int fileType = 0;
    
    // -------------------------------- reading error macro definition ---------------------------------
    #define onFileReadError()                                                                          \
        fprintf(stderr, "Error occured while reading file %s. The file may be corrupted\n", filename); \
        fsmod_files_close(&filesSt);                                                                   \
        return                                                                                         \
    // -------------------------------------------------------------------------------------------------

    switch(fileType = fsmod_files_init(&filesSt, filename)){
        case FSMOD_LEVEL_TYPE_STANDARD: {
            if(!(fileType & FSMOD_LEVEL_FLAG_NO_GFX)){
                u32 gfxEp = 0;
                if(!fread_LE_U32(&gfxEp, 1, filesSt.gfxPtrsFp)) onFileReadError();
                gfxEp = (u32)fsmod_infilePtrToOffset(gfxEp, filesSt.gfxChunkOffset);
                fseek(filesSt.gfxPtrsFp, gfxEp, SEEK_SET);

                if(!(fileType & FSMOD_LEVEL_FLAG_NO_TILES)){
                    // #---- Scan for tiles ----#
                    u32 tilesEp = 0;
                    if(!fread_LE_U32(&tilesEp, 1, filesSt.tilesPtrsFp)) onFileReadError();
                    tilesEp = (u32)fsmod_infilePtrToOffset(tilesEp, filesSt.tilesChunkOffset);
                    fseek(filesSt.tilesPtrsFp, tilesEp, SEEK_SET);
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
static int fsmod_files_init(struct fsmod_files * filesStp, char filename[]){
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

        //rewind(fp);
        // Tiles chunk setup TODO: check and test
        fseek(fp, 0x28, SEEK_SET /*TODO: check*/);
        fread_LE_U32(&filesStp->tilesChunkSize, 1, fp);
        fread_LE_U32(&filesStp->tilesChunkOffset, 1, fp);
        if(filesStp->tilesChunkOffset && filesStp->tilesChunkSize > 32 && filesStp->tilesChunkOffset + filesStp->tilesChunkSize <= fileSize){
            if(!(filesStp->tilesDataFp = fopen(filename, "rb")) 
            || !(filesStp->tilesPtrsFp = fopen(filename, "rb"))){
                fclose(fp);
                return -1;
            }
            // non-ptr int arithmetics below
            u32 epOffset = filesStp->tilesChunkOffset + filesStp->tilesChunkSize / 2048 + 4; //< entry point address for ptrs lookup
            fseek(filesStp->tilesDataFp, filesStp->tilesChunkOffset, SEEK_SET); 
            fseek(filesStp->tilesPtrsFp, epOffset, SEEK_SET);
        } 
        else retVal |= 2;
        // GFX chunk setup
        fseek(fp, 0x18, SEEK_CUR);
        fread_LE_U32(&filesStp->gfxChunkSize, 1, fp);
        fread_LE_U32(&filesStp->gfxChunkOffset, 1, fp);
        if(filesStp->gfxChunkOffset && filesStp->gfxChunkSize > 32 && filesStp->gfxChunkOffset + filesStp->gfxChunkSize <= fileSize){
            if(!(filesStp->gfxDataFp = fopen(filename, "rb")) 
            || !(filesStp->gfxPtrsFp = fopen(filename, "rb"))){
                fclose(fp);
                return -1;
            }
            // non-ptr int arithmetics below
            u32 epOffset = filesStp->gfxChunkOffset + filesStp->gfxChunkSize / 2048 + 4; //< entry point address for ptrs lookup
            fseek(filesStp->gfxDataFp, filesStp->gfxChunkOffset + filesStp->gfxChunkSize / 8192, SEEK_SET);
            fseek(filesStp->gfxPtrsFp, epOffset, SEEK_SET);
        }
        else retVal |= 4;
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

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset){
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 fsmod_offsetToInfilePtr(uptr offset, uptr startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}
