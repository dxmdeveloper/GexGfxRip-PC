#ifndef _FILESCANNING_H_
#define _FILESCANNING_H_ 1
#include <stdlib.h>
#include <stdint.h>
#include "gfx.h"

#define FILE_MIN_SIZE 128
typedef void (*scan_foundCallback_t)(void * bitmap, void * headerAndOpMap, const struct gfx_palette*, const char filename[]);

/// @brief scans file for gex graphics files
/// @param filename path to file to read
/// @param foundCallback callback function which is executed on every found graphic. 
/// takes 2 arguments: pointer to found graphic and pointer to assigned color palette (may be null ptr).
void fsmod_scan4Gfx(char filename[], scan_foundCallback_t);


/// @brief function scanning memory for u32 value
/// @param endPtr end of the scanning range. The offset is excluded from the scan.
/// @param ORMask logical OR mask. 0 by default
/// @param matchVal searched value.
/// @return offset of found value. null if not found.
uintptr_t findU32(void *startPtr, void *endPtr, uint32_t ORMask, uint32_t matchVal);


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
    uint32_t tilesChunkSize;
    uint32_t gfxChunkSize;
    uint32_t tilesChunkOffset;
    uint32_t gfxChunkOffset;
    uint32_t gfxChunkEp;
    uint32_t tilesChunkEp;

    jmp_buf error_jmp_buf;
};

/** @brief initializes fsmod_files structure. Opens one file in read mode multiple times and sets it at start position.
    @return enum fsmod_level_type with bit flags */

// filesStp->error_jmp_buf MUST be set before or after initialization
int fsmod_files_init(struct fsmod_files * filesStp, const char filename[]);
void fsmod_files_close(struct fsmod_files * filesStp);

#endif