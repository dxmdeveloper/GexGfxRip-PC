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
    #define MAKEDIR(x) _mkdir(x)
#else //POSIX
    #include <sys/stat.h>
    #define MAKEDIR(x) mkdir(x, 0755)
#endif

// ___________________________________________________ STRUCTURES ___________________________________________________



// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset);
static u32 fsmod_offsetToInfilePtr(uptr fileOffset, uptr startOffset);
//static void *matchColorPalette(void *gfxOffset, void *chunkStart, void *chunkEnd);

/** @brief checks file pointers for errors and eofs. if at least one has an error or eof flag jumps to error_jmp_buf
    @param mode 0 - check all, 1 - check only ptrsFps, 2 - check only dataFps */
static void fsmod_files_check_errors_and_eofs(struct fsmod_files * filesStp, int mode);

//! TO BE REMOVED
/** @brief reads one u32 value from both: tilesPtrsFp and gfxPtrsFp (in that order) to u32pairp. 
           Jumps to error_jmp_buf if cannot read the values */
inline static void fsmod_readU32_from_both_pFps(struct fsmod_files * filesStp, u32pair u32pairp[static 1]);

/** @brief reads infile ptr (aka gexptr) from file and converts it to file offset.
           Jumps to error_jmp_buf if cannot read the values */
inline static u32 fsmod_read_infile_ptr(FILE * fp, u32 chunkOffset, jmp_buf *error_jmp_buf);

/** @brief fread wrapper with error handling.
           Jumps to error_jmp_buf if cannot read the values */
inline static size_t fsmod_fread(void *dest, size_t size, size_t n, FILE * fp, jmp_buf *error_jmp_buf);

//! TO BE REMOVED
/** @brief reads one gexptr from both: tilesPtrsFp and gfxPtrsFp (in that order) & converts it to u32 offset. 
           Jumps to error_jmp_buf if cannot read the values */
inline static u32pair fsmod_read_infile_ptr_from_both(struct fsmod_files * filesStp);

//TODO: consider additional argument for filename
/** @brief reads tile header and bitmap into arrays, creates palette, calls callback 
    @param filesStp file pointers must be set at the correct positions (tile graphic entries) */
static void fsmod_prep_tile_gfx_data_and_exec_cb(struct fsmod_files * filesStp, scan_foundCallback_t cb);

/** @param nextOffset value returned by previous call of this function or 0 for the first call
    @return offsets to infile_ptr for next tiles set. First for tiles chunk, second for gfx chunk.
            first will be 0 if last set was just read.
            last will be ~0 there's no tile sets or error occured during file read. ferror & feof should be called */
static u32pair fsmod_head_to_tiles_records(struct fsmod_files * filesStp, u32pair nextOffsets);

// part of fsmod_init
static inline int _fsmod_files_init_open_and_set(const char filename[], FILE * mainFp,
 size_t fileSize, FILE ** ptrsFp, FILE ** dataFp, u32 * chunkSizep, u32 * chunkOffsetp, u32 * chunkEpp);


// ___________________________________________________ FUNCTION DEFINITIONS ___________________________________________________

// ------------------- Extern functions definitions: -------------------
// Main module function
/// TODO: replace with separate functions
void fsmod_scan4Gfx(char filename[], scan_foundCallback_t foundCallback){
    struct fsmod_files filesSt = {0};
    int fileType = 0;
    enum fsmod_file_read_errno_enum read_errno = 0;


    char * eFilename = malloc(strlen(filename)+201);

    // File reading error jump location
    if((read_errno = setjmp(*filesSt.error_jmp_buf))){
        fprintf(stderr, "Error occured while reading file %s. The file may be corrupted\n fsmod_file_read_errno: %i\n", filename, read_errno);
        if(eFilename) free(eFilename);
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
                    u32pair nextOffsets = {0};

                    //TODO GIVE OPTION NOT TO CREATE A DIR
                    sprintf(eFilename, "%s-rip", filename);
                    MAKEDIR(eFilename);
                    strcat(eFilename,"/tiles");
                    MAKEDIR(eFilename);

                    nextOffsets = fsmod_head_to_tiles_records(&filesSt, nextOffsets);
                    if(nextOffsets.first || nextOffsets.second != ~0){
                        do {
                            // DEBUG INFO
                            fprintf(stdout, "tile records block offset: %lX\n", ftell(filesSt.gfxPtrsFp));
                            fprintf(stdout, "tile bitmap pointers block offset: %lX\n", ftell(filesSt.tilesPtrsFp));

                            fseek(filesSt.gfxPtrsFp, 8, SEEK_CUR); // TODO: find out what first u32 value mean
                            u32values = fsmod_read_infile_ptr_from_both(&filesSt);
                            if(u32values.second != ~0){
                                do {
                                    fseek(filesSt.tilesDataFp, u32values.first, SEEK_SET);
                                    fseek(filesSt.gfxDataFp, u32values.second, SEEK_SET);

                                    fsmod_prep_tile_gfx_data_and_exec_cb(&filesSt, foundCallback);

                                    fseek(filesSt.gfxPtrsFp,8, SEEK_CUR);
                                    u32values = fsmod_read_infile_ptr_from_both(&filesSt);
                                } while(u32values.first != 0);
                            }

                            // next tile set
                            nextOffsets = fsmod_head_to_tiles_records(&filesSt, nextOffsets);
                        } while (nextOffsets.first);
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
    if(eFilename) free(eFilename);
    fsmod_files_close(&filesSt);
}


int fsmod_files_init(struct fsmod_files * filesStp, const char filename[]){
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
        // TODO: more special files detection
        retVal = 1;
    }

    if(fp != NULL) fclose(fp);
    return retVal;
}


void fsmod_files_close(struct fsmod_files * filesStp){
    if(filesStp->gfxDataFp) {fclose(filesStp->gfxDataFp); filesStp->gfxDataFp = NULL;}
    if(filesStp->gfxPtrsFp) {fclose(filesStp->gfxPtrsFp); filesStp->gfxPtrsFp = NULL;}
    if(filesStp->tilesDataFp) {fclose(filesStp->tilesDataFp); filesStp->tilesDataFp = NULL;}
    if(filesStp->tilesPtrsFp) {fclose(filesStp->tilesPtrsFp); filesStp->tilesPtrsFp = NULL;}
}


int fsmod_follow_pattern(FILE* fp, u32 chunkOffset, const char pattern[], jmp_buf * error_jmp_buf){
    u32 u32val = 0;
    int intVal = 0;

    for(const char* pcur = pattern; *pcur; pcur++){
        switch(*pcur){
            case 'g':{
                u32val = fsmod_read_infile_ptr(fp, chunkOffset, error_jmp_buf);
                if(!u32val) return EXIT_FAILURE;
                fseek(fp, SEEK_SET, u32val);
            } break;
            case '+': pcur+=1; // no break
            case '-': {
                intVal = atoi(pcur);
                fseek(fp, SEEK_CUR, intVal);
            } break;
        }
    }
    return EXIT_SUCCESS;
}

// ------------------- Static functions definitions: -------------------

inline static void fsmod_readU32_from_both_pFps(struct fsmod_files * filesStp, u32pair u32pairp[static 1]){                        
    if(!fread_LE_U32(&u32pairp->first, 1, filesStp->tilesPtrsFp)) 
        longjmp(*filesStp->error_jmp_buf, FSMOD_READ_ERROR_FREAD);        
    if(!fread_LE_U32(&u32pairp->second, 1, filesStp->gfxPtrsFp))
        longjmp(*filesStp->error_jmp_buf, FSMOD_READ_ERROR_FREAD);
}

inline static u32 fsmod_read_infile_ptr(FILE * fp, u32 chunkOffset, jmp_buf *error_jmp_buf){
    u32 val = 0;
    if(!fread_LE_U32(&val, 1, fp))
        longjmp(*error_jmp_buf, FSMOD_READ_ERROR_FREAD);

    return (u32)fsmod_infilePtrToOffset(val, chunkOffset);
}

inline static size_t fsmod_fread(void *dest, size_t size, size_t n, FILE * fp, jmp_buf *error_jmp_buf){
    size_t retval = fread(dest, size, n, fp);
    if(retval < n)
        longjmp(*error_jmp_buf, FSMOD_READ_ERROR_FREAD);
    return retval;
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

//  --- part of fsmod_files_init ---
/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static int _fsmod_files_init_open_and_set(const char filename[], FILE * mainFp, size_t fileSize, FILE ** ptrsFp, FILE ** dataFp, u32 * chunkSizep, u32 * chunkOffsetp, u32 * chunkEpp){
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

static u32pair fsmod_head_to_tiles_records(struct fsmod_files * filesStp, u32pair nextOffset){
    u32pair u32vals = {0};
    u32pair retVals = {0};

    if(!nextOffset.first){
        // TILES CHUNK FILE POINTER SETUP
        fseek(filesStp->tilesPtrsFp, filesStp->tilesChunkEp, SEEK_SET);
        // GFX CHUNK FILE POINTER SETUP
        fseek(filesStp->gfxPtrsFp, (filesStp->gfxChunkEp + 0x28), SEEK_SET);

        if(!fread_LE_U32(&u32vals.first, 1, filesStp->gfxPtrsFp)) 
            longjmp(*filesStp->error_jmp_buf, FSMOD_READ_ERROR_FREAD);

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

static void fsmod_prep_tile_gfx_data_and_exec_cb(struct fsmod_files * filesStp, scan_foundCallback_t cb){
    void * bitmap = NULL;
    void * header = NULL;
    struct gfx_palette pal = {0};
    size_t bitmap_size = 0, required_size = 0;
    u16 tileWidthAndHeight[2] = {0};
    u32 offset = 0;
    char suggestedName[81] = ""; //temp

    jmp_buf loc_error_jmp_buf; int errno = 0;

    // error catch jump 
    if((errno = setjmp(loc_error_jmp_buf))){
        if(header) free(header);
        if(bitmap) free(bitmap);
        longjmp(*filesStp->error_jmp_buf, errno);
    }

    fread_LE_U16(tileWidthAndHeight, 2, filesStp->tilesDataFp); // TODO: error handling
    bitmap_size = tileWidthAndHeight[0] * 2 * tileWidthAndHeight[1];

    if(!gex_gfxHeadersFToAOB(filesStp->gfxDataFp, &header)) /* <- mem allocated */{
        fprintf(stderr,"bad header at: %lu\n", ftell(filesStp->gfxDataFp));
        if(header) free(header); header = NULL;
        //longjmp(filesStp->error_jmp_buf, FSMOD_READ_ERROR_WRONG_VALUE);
        return; // skip
    } 
    required_size = gfx_checkSizeOfBitmap(header);
    //! TESTS 
    // TODO: fix bitmap size calc
    bitmap_size *= 2; // ?????

    if(/*bitmap_size >= required_size &&*/ required_size){
        // bitmap data read
        if(!(bitmap = malloc(required_size))) exit(0xbeef);
        fsmod_fread(bitmap, 1, required_size, filesStp->tilesDataFp, &loc_error_jmp_buf); 

        // palette read
        offset = fsmod_read_infile_ptr(filesStp->gfxPtrsFp, filesStp->gfxChunkOffset, &loc_error_jmp_buf);
        fseek(filesStp->gfxDataFp, offset, SEEK_SET);
        gfx_palette_parsef(filesStp->gfxDataFp, &pal);

        // preparing filename
        sprintf(suggestedName, "%u", offset); // temp?

        // CALLING ONFOUND CALLBACK
        cb(bitmap, header, &pal, suggestedName);
    }

    if(header) free(header);
    if(bitmap) free(bitmap);
}

static uptr fsmod_infilePtrToOffset(u32 infile_ptr, uptr startOffset){
    if(infile_ptr == 0) return 0;
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 fsmod_offsetToInfilePtr(uptr offset, uptr startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}
