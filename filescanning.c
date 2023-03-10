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



// STATIC DECLARATIONS:
static uintptr_t infilePtrToOffset(u32 infile_ptr, uintptr_t startOffset);
static u32 offsetToInfilePtr(uintptr_t fileOffset, uintptr_t startOffset);
//static void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd);
static void scanChunk(void *startOffset, void *endOffset, scan_foundCallback_t, char path[], u32 pathLen, uintptr_t inf_fileDataAlloc);

// GLOBAL DEFINITIONS:
void scan4Gfx(char filename[], scan_foundCallback_t foundCallback){
    FILE * fp = NULL;
    u32 fileChunksCount = 0;
    size_t fileSize = 0;

    fp = fopen(filename, "rb");
    if(fp == NULL){
        fprintf(stderr, "Cannot open file: %s", filename);
        return;
    }
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);

    if(fileSize < FILE_MIN_SIZE) return;
    //read first value
    if(!fread_LE_U32(&fileChunksCount, 1, fp)) return;

    // Check file type
    if(fileChunksCount >= 5 && fileChunksCount <= 32){
        //FILE TYPE: LEVEL
        FILE * tilesDataFp = NULL;
        FILE * tilesPtrsFp = NULL;
        FILE * gfxDataFp = NULL;
        FILE * gfxPtrsFp = NULL;
        u32 tilesChunkSize = 0;
        u32 gfxChunkSize = 0;
        u32 tilesChunkOffset = 0;
        u32 gfxChunkOffset = 0;

        //rewind(fp);
        // Tiles chunk setup TODO: check and test
        fseek(fp, 0x28, SEEK_SET /*check*/);
        fread_LE_U32(&tilesChunkSize, 1, fp);
        fread_LE_U32(&tilesChunkOffset, 1, fp);
        if(tilesChunkOffset && tilesChunkSize > 32 && tilesChunkOffset + tilesChunkSize <= fileSize){
            if(!(tilesDataFp = fopen(filename, "rb")) 
            || !(tilesPtrsFp = fopen(filename, "rb"))){
                fclose(fp);
                fprintf(stderr, "Cannot open: %s", filename);
                return;
            }
            fseek(tilesDataFp, tilesChunkOffset, SEEK_SET);
            fseek(tilesPtrsFp, tilesChunkOffset, SEEK_SET);
        }
        // GFX chunk setup
        fseek(fp, 0x18, SEEK_CUR);
        fread_LE_U32(&gfxChunkSize, 1, fp);
        fread_LE_U32(&gfxChunkOffset, 1, fp);
        if(gfxChunkOffset && gfxChunkSize > 32 && gfxChunkOffset + gfxChunkSize <= fileSize){
            if(!(gfxDataFp = fopen(filename, "rb")) 
            || !(gfxPtrsFp = fopen(filename, "rb"))){
                fclose(fp);
                fprintf(stderr, "Cannot open: %s", filename);
                return;
            }
            fseek(gfxDataFp, gfxChunkOffset, SEEK_SET);
            fseek(gfxPtrsFp, gfxChunkOffset, SEEK_SET);
        } 
        // TODO: consider function for above

        // close fp pointer
        fclose(fp); fp = NULL;

        // TODO: TESTS and then scanning
        /*
         scanning here
        */

        // files close
        if(tilesDataFp != NULL) fclose(tilesDataFp);
        if(tilesPtrsFp != NULL) fclose(tilesPtrsFp);
        if(gfxDataFp != NULL) fclose(gfxDataFp);
        if(gfxPtrsFp != NULL) fclose(gfxPtrsFp);
    }
    if(fp != NULL) fclose(fp);
}

// STATIC DEFINITIONS:
static uintptr_t infilePtrToOffset(u32 infile_ptr, uintptr_t startOffset){
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 offsetToInfilePtr(uintptr_t offset, uintptr_t startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}
