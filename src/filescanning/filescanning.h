#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include "filescanning_tiles.h"
#include "filescanning_obj_gfx_and_bg.h"
#include "../graphics/gfx.h"
#include "../essentials/vector.h"
#include "../essentials/stack.h"
#include "../essentials/ptr_map.h"

#define FILE_MIN_SIZE 128

// ONE USE PER CODE BLOCK!
#define FSCAN_ERRBUF_CHAIN_ADD(errbufpp, extension_code)                \
    jmp_buf new_error_jump_buffor; int errbuf_errno = 0;                \
    jmp_buf *prev_error_jump_bufforp = NULL;                            \
    jmp_buf *additional_error_jump_buffor_ptr = &new_error_jump_buffor; \
    jmp_buf **bufpp = errbufpp;                                         \
    if((bufpp)) prev_error_jump_bufforp = *(bufpp);                     \
    if(!(bufpp)) bufpp = & additional_error_jump_buffor_ptr;            \
    *(bufpp) = &new_error_jump_buffor;                                  \
    if((errbuf_errno = setjmp(new_error_jump_buffor))){                 \
        extension_code                                                  \
        *(bufpp) = prev_error_jump_bufforp;                             \
        if(*(bufpp)) longjmp(**(bufpp), errbuf_errno);                  \
        else exit(errbuf_errno);                                        \
    }

#define FSCAN_ERRBUF_REVERT(bufpp) if(bufpp) *(bufpp) = prev_error_jump_bufforp

/* ERRORS: -1 - failed to open a file, -2 - file is too small, -3 read error, 
   TYPES: 0 - loaded standard level file, 1 - loaded standalone gfx file
   BIT FLAGS: 2 - level file does not contain valid tiles chunk, 4 - level file does not contain valid Gfx chunk. */
enum fscan_level_type_enum {
    FSCAN_LEVEL_TYPE_FOPEN_ERROR    = -1,
    FSCAN_LEVEL_TYPE_FILE_TOO_SMALL = -2,
    FSCAN_LEVEL_TYPE_FREAD_ERROR    = -3,
    FSCAN_LEVEL_TYPE_STANDARD       = 0,
    FSCAN_LEVEL_TYPE_GFX_ONLY       = 1,
    FSCAN_LEVEL_FLAG_NO_TILES  = 1 << 1,
    FSCAN_LEVEL_FLAG_NO_MAIN   = 1 << 2,
    FSCAN_LEVEL_FLAG_NO_BACKGROUND = 1 << 3,
};

enum fscan_errno_enum {
    FSCAN_READ_NO_ERROR = 0,
    FSCAN_READ_ERROR_FERROR,
    FSCAN_READ_ERROR_FREAD,
    FSCAN_READ_ERROR_FOPEN,
    FSCAN_READ_ERROR_NO_EP,
    FSCAN_READ_ERROR_INVALID_POINTER,
    FSCAN_READ_ERROR_UNEXPECTED_NULL,
    FSCAN_READ_ERROR_UNEXPECTED_EOF,
    FSCAN_READ_ERROR_WRONG_VALUE,
    FSCAN_ERROR_INDEX_OUT_OF_RANGE,
};
typedef struct fscan_file_chunk_structure {
    FILE * data_fp;
    FILE * ptrs_fp;
    size_t size;
    uint32_t offset;
    uint32_t ep;
} fscan_file_chunk; 

struct fscan_files {
    fscan_file_chunk tile_chunk;
    fscan_file_chunk bitmap_chunk;
    fscan_file_chunk bg_chunk;
    fscan_file_chunk main_chunk;
    fscan_file_chunk intro_chunk;
    
    uint32_t ext_bmp_index;
    gexdev_u32vec ext_bmp_offsets;
    bool used_fchunks_arr[6];

    jmp_buf* error_jmp_buf;
};

/** @brief reads infile ptr (aka gexptr) from file and converts it to file offset.
           Jumps to error_jmp_buf if cannot read the values */
uint32_t fscan_read_infile_ptr(FILE * fp, uint32_t chunkOffset, jmp_buf *error_jmp_buf);

/** @brief fread wrapper with error handling.
           Jumps to error_jmp_buf if cannot read the values */
size_t fscan_fread(void *dest, size_t size, size_t n, FILE * fp, jmp_buf *error_jmp_buf);

/** @brief initializes fscan_files structure. Opens one file in read mode multiple times and sets it at start position.
    @return enum fscan_level_type with bit flags */
// filesStp->error_jmp_buf MUST be set before or after initialization
int fscan_files_init(struct fscan_files * filesStp, const char filename[]);
void fscan_files_close(struct fscan_files * filesStp);


/////** @brief checks file pointers for errors and eofs. if at least one has an error or eof flag jumps to error_jmp_buf
////    @param mode 0 - check all, 1 - check only ptrsFps, 2 - check only dataFps */
////void fscan_files_check_errors_and_eofs(struct fscan_files filesStp[static 1], int mode);

/** @brief made to be used with fscan_follow_pattern_recur.
  * @param clientp gexdev_u32vec vec[2]. First for pointing block of tile bitmaps start indexes in second vector.
  * The second vector keeps offsets of tile bitmaps */
int fscan_cb_read_offset_to_vec_2lvls(fscan_file_chunk * chunkp, gexdev_u32vec * iter, uint32_t * ivars, void * clientp);

/** @brief allocates memory and read header and raw bitmap.
  * If bitmap is in bmpchunkp (the function detects it automatically)
  * and graphic is segmented into multiple chunks then another bitmaps are joined.
  *
  * @param chunkp file chunk with header (may contain bitmap as well). ptrs_fp must be set at header offset value.
  * position of ptrs_fp will be moved by 4.
  *
  * @param extbmpchunkp file chunk with external bitmaps. ptrs_fp will not be moved.
  * @param header_and_bitmapp pointer to array that is destined to contain header data.
  * NOTICE: If function fail pointer can be set to NULL.
  * IMPORTANT: array must be freed outside this function.
  *
  * @param bmp_startpp pointer to pointer to bitmap in header_and_bitmapp. Can be NULL.
  * @return size of header_and_bitmap array. 0 means that function failed. */
size_t fscan_read_header_and_bitmaps_alloc(fscan_file_chunk *chunkp, fscan_file_chunk *extbmpchunkp, void **header_and_bitmapp,
                                           void **bmp_startpp, const uint32_t ext_bmp_offsets[], size_t ext_bmp_offsets_size,
                                           unsigned int *bmp_indexp, jmp_buf *errbufp, gexdev_ptr_map *header_bmp_bindsp);
