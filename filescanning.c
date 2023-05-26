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

// ___________________________________________________ STRUCTURES ___________________________________________________

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

enum fsmod_file_read_errno_enum {
    FSMOD_READ_NO_ERROR = 0,
    FSMOD_READ_ERROR_FERROR,
    FSMOD_READ_ERROR_FREAD,
    FSMOD_READ_ERROR_FOPEN,
    FSMOD_READ_ERROR_NO_EP,
    FSMOD_READ_ERROR_INVALID_POINTER,
    FSMOD_READ_ERROR_UNEXPECTED_NULL,
    FSMOD_READ_ERROR_UNEXPECTED_EOF,
    FSMOD_READ_ERROR_WRONG_VALUE,
};

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

    jmp_buf error_jmp_buf;
};

// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset);
static u32 fsmod_offsetToInfilePtr(uptr fileOffset, uptr startOffset);
//static void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd);

/** @brief initializes fsmod_files structure. Opens one file in read mode multiple times and sets it at start position.
    @return enum fsmod_level_type with bit flags */
static int fsmod_files_init(struct fsmod_files * filesStp, const char filename[]);
static void fsmod_files_close(struct fsmod_files * filesStp);

/** @brief checks file pointers for errors and eofs. if at least one has an error or eof flag jumps to error_jmp_buf
    @param mode 0 - check all, 1 - check only ptrsFps, 2 - check only dataFps */
static void fsmod_files_check_errors_and_eofs(struct fsmod_files * filesStp, int mode);

/** @brief reads one u32 value from both: tilesPtrsFp and gfxPtrsFp (in that order) to u32pairp. 
           jumps to error_jmp_buf if cannot read the values */
inline static void fsmod_readU32_from_both_pFps(struct fsmod_files * filesStp, u32pair u32pairp[static 1]);

/** @brief reads infile ptr (aka gexptr) from file and converts it to file offset
           jumps to error_jmp_buf if cannot read the values */
inline static u32 fsmod_read_infile_ptr(FILE * fp, u32 chunkOffset, jmp_buf error_jmp_buf);

/** @brief reads one gexptr from both: tilesPtrsFp and gfxPtrsFp (in that order) & converts it to u32 offset. 
           jumps to error_jmp_buf if cannot read the values */
inline static u32pair fsmod_read_infile_ptr_from_both(struct fsmod_files * filesStp);

/** @param nextOffset value returned by previous call of this function or 0 for the first call
    @return offsets to infile_ptr for next tiles set. First for tiles chunk, second for gfx chunk.
            first will be 0 if last set was just read.
            last will be ~0 there's no tile sets or error occured during file read. ferror & feof should be called */
static u32pair fsmod_head_to_tiles_records(struct fsmod_files * filesStp, u32pair nextOffsets);



// ___________________________________________________ FUNCTION DEFINITIONS ___________________________________________________

// ------------------- Extern functions definitions: -------------------
// Main module function
void fsmod_scan4Gfx(char filename[], scan_foundCallback_t foundCallback){
    struct fsmod_files filesSt = {0};
    int fileType = 0;
    enum fsmod_file_read_errno_enum read_errno = 0;

    // File reading error jump location
    if((read_errno = setjmp(filesSt.error_jmp_buf))){
        fprintf(stderr, "Error occured while reading file %s. The file may be corrupted\n fsmod_file_read_errno: %i\n", filename, read_errno);
        fsmod_files_close(&filesSt);
        return;
    }
    // --------------------------------


    switch(fileType = fsmod_files_init(&filesSt, filename)){
        case FSMOD_LEVEL_TYPE_STANDARD: {
            if(!(fileType & FSMOD_LEVEL_FLAG_NO_GFX)){

                if(!(fileType & FSMOD_LEVEL_FLAG_NO_TILES)){
                    // #---- Scan for tiles ----#
                    u32pair u32values = {0};
                    u32values = fsmod_head_to_tiles_records(&filesSt, u32values);
                    if(u32values.first || u32values.second != ~0){
                        do {
                            /// ! TESTS
                            fprintf(stdout, "tile records block offset: %lX\n", ftell(filesSt.gfxPtrsFp));
                            fprintf(stdout, "tile bitmap pointers block offset: %lX\n", ftell(filesSt.tilesPtrsFp));

                            fseek(filesSt.gfxPtrsFp, 4, SEEK_CUR); // TODO: find out what first u32 value mean

                            // next tile set
                            u32values = fsmod_head_to_tiles_records(&filesSt, u32values);
                        } while (u32values.first);
                    }
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
}

// ------------------- Static functions definitions: -------------------

inline static void fsmod_readU32_from_both_pFps(struct fsmod_files * filesStp, u32pair u32pairp[static 1]){                        
    if(!fread_LE_U32(&u32pairp->first, 1, filesStp->tilesPtrsFp)) 
        longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_FREAD);        
    if(!fread_LE_U32(&u32pairp->second, 1, filesStp->gfxPtrsFp))
        longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_FREAD);
}

inline static u32 fsmod_read_infile_ptr(FILE * fp, u32 chunkOffset, jmp_buf error_jmp_buf){
    u32 val = 0;
    if(!fread_LE_U32(&val, 1, fp))
        longjmp(error_jmp_buf, FSMOD_READ_ERROR_FREAD);

    return (u32)fsmod_infilePtrToOffset(val, chunkOffset);
}

inline static u32pair fsmod_read_infile_ptr_from_both(struct fsmod_files * filesStp){
    u32pair pair = {0};
    pair.first = fsmod_read_infile_ptr(filesStp->tilesPtrsFp, filesStp->tilesChunkOffset, filesStp->error_jmp_buf);
    pair.second = fsmod_read_infile_ptr(filesStp->gfxPtrsFp, filesStp->gfxChunkOffset, filesStp->error_jmp_buf);
    return pair;
}


static void fsmod_files_check_errors_and_eofs(struct fsmod_files * filesStp, int mode){
    switch(mode){
        case 0:
        case 1:
            if(feof(filesStp->tilesPtrsFp) || feof(filesStp->gfxPtrsFp))
                longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_UNEXPECTED_EOF);
            if(ferror(filesStp->tilesPtrsFp) || ferror(filesStp->gfxPtrsFp)) 
                longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_FERROR);
            if(mode) return;
        case 2:
            if(feof(filesStp->tilesDataFp) || feof(filesStp->gfxDataFp))
                longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_UNEXPECTED_EOF);
            if(ferror(filesStp->tilesDataFp) || ferror(filesStp->gfxDataFp)) 
                longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_FERROR);
            break;
    }
}

//  --- part of fsmod_files_init ---
/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static inline int _fsmod_files_init_open_and_set(const char filename[], FILE * mainFp, size_t fileSize, FILE ** ptrsFp, FILE ** dataFp, u32 * chunkSizep, u32 * chunkOffsetp, u32 * chunkEpp){
    fread_LE_U32(chunkSizep, 1, mainFp);
    fread_LE_U32(chunkOffsetp, 1, mainFp);
    if(!(*chunkOffsetp && *chunkSizep > 32 && *chunkOffsetp + *chunkSizep <= fileSize)) return 1;
    if(!(*dataFp = fopen(filename, "rb")) 
    || !(*ptrsFp = fopen(filename, "rb"))){
        return -1;
    }
    // non-ptr int arithmetics below.  Setting initial stream positions & Entry point offset
    u32 epOffset = *chunkOffsetp + *chunkSizep / 2048 + 4; //< entry point address for ptrs lookup
    fseek(*dataFp, *chunkOffsetp, SEEK_SET); 
    fseek(*ptrsFp, epOffset, SEEK_SET);
    fread_LE_U32(chunkEpp, 1, *ptrsFp);
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
                                          &filesStp->tilesPtrsFp, &filesStp->tilesDataFp,&filesStp->tilesChunkSize,
                                          &filesStp->tilesChunkOffset,&filesStp->tilesChunkEp)) {
            case -1: fclose(fp); return -1; // failed to open the file again
            case 1: retVal |= 2; break; // invalid / non-exsiting chunk
        }
        // GFX chunk setup
        fseek(fp, 0x18, SEEK_CUR);
        switch(_fsmod_files_init_open_and_set(filename, fp, fileSize,
                                          &filesStp->gfxPtrsFp, &filesStp->gfxDataFp,&filesStp->gfxChunkSize,
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
    u32pair u32vals = {0};
    u32pair retVals = {0};

    if(!nextOffset.first){
        // TILES CHUNK FILE POINTER SETUP
        fseek(filesStp->tilesPtrsFp, filesStp->tilesChunkEp, SEEK_SET);
        // GFX CHUNK FILE POINTER SETUP
        fseek(filesStp->gfxPtrsFp, (filesStp->gfxChunkEp + 0x28), SEEK_SET);

        if(!fread_LE_U32(&u32vals.first, 1, filesStp->gfxPtrsFp)) 
            longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_FREAD);

        fseek(filesStp->gfxPtrsFp, fsmod_infilePtrToOffset(u32vals.first, filesStp->gfxChunkOffset), SEEK_SET);
    }
    else {
        fseek(filesStp->tilesPtrsFp, nextOffset.first, SEEK_SET);
        fseek(filesStp->gfxPtrsFp, nextOffset.second, SEEK_SET);
    }

    u32vals = fsmod_read_infile_ptr_from_both(filesStp);
    if(u32vals.first == 0 || u32vals.second == 0) return (u32pair){0,~0};
    fsmod_readU32_from_both_pFps(filesStp, &retVals); // read next values

    if(retVals.first != 0 || retVals.second != 0){
        retVals.first = (u32)ftell(filesStp->tilesPtrsFp) - 4;
        retVals.second = (u32)ftell(filesStp->gfxPtrsFp) - 4;
    }

    // FSEEK TO DESTINITION
    fseek(filesStp->tilesPtrsFp, u32vals.first, SEEK_SET);
    fseek(filesStp->gfxPtrsFp, u32vals.second, SEEK_SET);

    // ADDITIONAL FILES ERROR CHECK
    fsmod_files_check_errors_and_eofs(filesStp, 1);

    return retVals;
}
static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset){
    if(infile_ptr == 0) return 0;
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 fsmod_offsetToInfilePtr(uptr offset, uptr startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}


#undef readU32_from_both