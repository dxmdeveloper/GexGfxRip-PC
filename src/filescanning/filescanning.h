#ifndef _FILESCANNING_H_
#define _FILESCANNING_H_ 1
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include "filescanning_tiles.h"
#include "../graphics/gfx.h"
#include "../essentials/vector.h"
#include "../essentials/stack.h"

#define FILE_MIN_SIZE 128

// ONE USE PER CODE BLOCK!
#define FSMOD_ERRBUF_EXTEND(bufp, extension_code)               \
    jmp_buf new_error_jump_buffor; int errbuf_errno = 0;        \
    jmp_buf * prev_error_jump_bufforp = bufp;                   \
    bufp = &new_error_jump_buffor;                              \
    if((errbuf_errno = setjmp(new_error_jump_buffor))){         \
        extension_code                                          \
        bufp = prev_error_jump_bufforp;                         \
        if(bufp) longjmp(*bufp, errbuf_errno);                  \
        else exit(errbuf_errno);                                \
    }

#define FSMOD_ERRBUF_REVERT(bufp) bufp = prev_error_jump_bufforp


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

enum fsmod_errno_enum {
    FSMOD_READ_NO_ERROR = 0,
    FSMOD_READ_ERROR_FERROR,
    FSMOD_READ_ERROR_FREAD,
    FSMOD_READ_ERROR_FOPEN,
    FSMOD_READ_ERROR_NO_EP,
    FSMOD_READ_ERROR_INVALID_POINTER,
    FSMOD_READ_ERROR_UNEXPECTED_NULL,
    FSMOD_READ_ERROR_UNEXPECTED_EOF,
    FSMOD_READ_ERROR_WRONG_VALUE,
    FSMOD_ERROR_INDEX_OUT_OF_RANGE,
};
typedef struct fsmod_file_chunk_structure {
    FILE * dataFp;
    FILE * ptrsFp;
    size_t size;
    uint32_t offset;
    uint32_t ep;
} fsmod_file_chunk; 

struct fsmod_files {
    fsmod_file_chunk tilesChunk;
    fsmod_file_chunk mainChunk;

    jmp_buf* error_jmp_buf;
};

/** @brief reads infile ptr (aka gexptr) from file and converts it to file offset.
           Jumps to error_jmp_buf if cannot read the values */
uint32_t fsmod_read_infile_ptr(FILE * fp, uint32_t chunkOffset, jmp_buf *error_jmp_buf);

/** @brief fread wrapper with error handling.
           Jumps to error_jmp_buf if cannot read the values */
size_t fsmod_fread(void *dest, size_t size, size_t n, FILE * fp, jmp_buf *error_jmp_buf);

/** @brief initializes fsmod_files structure. Opens one file in read mode multiple times and sets it at start position.
    @return enum fsmod_level_type with bit flags */
// filesStp->error_jmp_buf MUST be set before or after initialization
int fsmod_files_init(struct fsmod_files * filesStp, const char filename[]);
void fsmod_files_close(struct fsmod_files * filesStp);


/////** @brief checks file pointers for errors and eofs. if at least one has an error or eof flag jumps to error_jmp_buf
////    @param mode 0 - check all, 1 - check only ptrsFps, 2 - check only dataFps */
////void fsmod_files_check_errors_and_eofs(struct fsmod_files filesStp[static 1], int mode);

/** @brief made to be used with fsmod_follow_pattern_recur.
  * @param clientp gexdev_u32vec vec[2]. First for pointing block of tile bitmaps start indexes in second vector.
  * The second vector keeps offsets of tile bitmaps */
int fsmod_cb_read_offset_to_vec_2lvls(fsmod_file_chunk * chunkp, gexdev_u32vec * iter, void * clientp);


#endif