#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include "tiles.h"
#include "obj_gfx_and_bg.h"
#include "../graphics/gfx.h"
#include "../essentials/vector.h"
#include "../essentials/paged_map.h"
#include "../essentials/bitflag_array.h"
#include "../helpers/basicdefs.h"

#define FILE_MIN_SIZE 64
#define TILE_BMP_MAX_CHUNKS 16

/* ERRORS: -1 - failed to open a file, -2 - file is too small, -3 read error, 
   TYPES: 0 - loaded standard level file, 1 - loaded standalone gfx file
   BIT FLAGS: 2 - level file does not contain valid tiles chunk, 4 - level file does not contain valid Gfx chunk. */
enum fscan_level_type_enum
{
    FSCAN_LEVEL_TYPE_FOPEN_ERROR = -1,
    FSCAN_LEVEL_TYPE_FILE_TOO_SMALL = -2,
    FSCAN_LEVEL_TYPE_FREAD_ERROR = -3,
    FSCAN_LEVEL_TYPE_STANDARD = 0,
    FSCAN_LEVEL_TYPE_GFX_ONLY = 1,
    FSCAN_LEVEL_FLAG_NO_TILES = 1 << 1,
    FSCAN_LEVEL_FLAG_NO_MAIN = 1 << 2,
    FSCAN_LEVEL_FLAG_NO_INTRO = 1 << 3,
    FSCAN_LEVEL_FLAG_NO_BACKGROUND = 1 << 4,
};

enum fscan_errno_enum
{
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

typedef struct fscan_file_chunk
{
    FILE *data_fp;
    FILE *fp;
    size_t size;
    uint32_t offset;
    uint32_t ep;
} fscan_file_chunk;

typedef struct fscan_files
{
    fscan_file_chunk tile_bmp_chunk;
    fscan_file_chunk bitmap_chunk;
    fscan_file_chunk bg_chunk;
    fscan_file_chunk main_chunk;
    fscan_file_chunk intro_chunk;

    uint32_t ext_bmp_counter;
    gexdev_u32vec ext_bmp_offsets;
    gexdev_u32vec tile_ext_bmp_offsets[TILE_BMP_MAX_CHUNKS];

    int last_scanned_chunk; // 0 - main (tiles also), 1 - intro, 2 - background

    bool option_verbose;

    jmp_buf *error_jmp_buf;
} fscan_files;

typedef struct fscan_gfx_info
{
    uint8_t iteration[4];
    uint8_t chunk_count;
    uint32_t *ext_bmp_offsets; // array of length chunk_count
    struct gfx_properties gfx_props;
    uint32_t gfx_offset;
    uint32_t palette_offset;
} fscan_gfx_info;

#ifndef FSCAN_GFX_INFO_VEC_TYPEDEF
#define FSCAN_GFX_INFO_VEC_TYPEDEF 1
typedef gexdev_univec fscan_gfx_info_vec;
#endif

/// @brief function that returns pointer to fscan_gfx_info object at index.
/// @return NULL if index is out of range, valid pointer otherwise.
fscan_gfx_info *fscan_gfx_info_vec_at(const fscan_gfx_info_vec *vecp, size_t index);

void fscan_gfx_info_close(fscan_gfx_info *ginf);

/** @brief Closes vector of fscan_gfx_info objects */
void fscan_scan_result_close(fscan_gfx_info_vec *result);

/** @brief The same as scan_result_close. Closes vector of fscan_gfx_info objects */
void fscan_gfx_info_vec_close(fscan_gfx_info_vec *vecp);

/** @brief reads infile ptr (aka gexptr) from file and converts it to file offset.
           Jumps to error_jmp_buf if cannot read the values */
uint32_t fscan_read_gexptr(FILE *fp, uint32_t chunk_offset, jmp_buf *error_jmp_buf);

/** @brief fread wrapper with error handling.
           Jumps to error_jmp_buf if cannot read the values */
size_t fscan_fread(void *dest, size_t size, size_t n, FILE *fp, jmp_buf *error_jmp_buf);

/** @brief initializes fscan_files structure. Opens one file in read mode multiple times and sets it at start position.
    @return enum fscan_level_type with bit flags */
// filesStp->error_jmp_buf MUST be set before or after initialization
int fscan_files_init(fscan_files *files_stp, const char filename[]);
void fscan_files_close(fscan_files *files_stp);

/////** @brief checks file pointers for errors and eofs. if at least one has an error or eof flag jumps to error_jmp_buf
////    @param mode 0 - check all, 1 - check only ptrsFps, 2 - check only dataFps */
////void fscan_files_check_errors_and_eofs(struct fscan_files filesStp[static 1], int mode);

/** @brief made to be used with fscan_follow_pattern_recur.
  * @param clientp gexdev_u32vec vec[2]. First for pointing block of tile bitmaps start indexes in second vector.
  * The second vector keeps offsets of tile bitmaps */
int fscan_cb_read_offset_to_vec_2lvls(fscan_file_chunk *chunkp, gexdev_u32vec *iter, uint32_t *ivars, void *clientp);

/** @brief allocates memory and read header and raw bitmap.
  * If bitmap is in bmpchunkp (the function detects it automatically)
  * and graphic is segmented into multiple chunks then another bitmaps are joined.
  *
  * @param fchp file chunk with header (may contain bitmap as well). ptrs_fp must be set at header offset value.
  * position of ptrs_fp will be moved by 4.
  *
  * @param extbmpchunkp file chunk with external bitmaps. ptrs_fp will not be moved.
  * @param header_and_bitmappp pointer to array that is destined to contain header data.
  * NOTICE: If function fail pointer can be set to NULL.
  * IMPORTANT: array must be freed outside this function.
  *
  * @param bmp_startpp pointer to pointer to bitmap in header_and_bitmapp. Can be NULL.
  * @return size of header_and_bitmap array. 0 means that function failed. */
size_t fscan_read_header_and_bitmaps_alloc(fscan_file_chunk *fchp,
                                           fscan_file_chunk *extbmpchunkp,
                                           void **header_and_bitmappp,
                                           void **bmp_startpp,
                                           const u32 ext_bmp_offsets[],
                                           size_t ext_bmp_offsets_size,
                                           unsigned int *bmp_indexp,
                                           jmp_buf(*errbufp),
                                           gexdev_paged_map *header_bmp_bindsp);

uint32_t fscan_read_gexptr_and_follow(fscan_file_chunk *fchp, int addoff, jmp_buf(*errbufp));

/** @brief reads null terminated array of gexptrs and converts them to file offsets. The function ensures that read pointer has a valid address.
 * @param dest array wherein resolved file offsets will be written. The function will write 0 as last pointer
 * @param dest_size size of dest array. Minimum allowed value is 1, but should be minimum 2
 * @return count of read pointers */
size_t fscan_read_gexptr_null_term_arr(fscan_file_chunk *fchp, uint32_t dest[], size_t dest_size, jmp_buf(*errbufp));

/** @brief search bitmap chunk for bitmap of game objects and background tiles and pushes offsets to files_stp->ext_bmp_offsets vector
 *  Will not search at all if the chunk is missing or is already scanned.
 *  @return Pointer to files_stp->ext_bmp_offsets. */
const gexdev_u32vec *fscan_search_for_ext_bmps(fscan_files *files_stp);

/** @brief search tile bmp chunk for tile bitmaps and pushes offsets to files_stp->tile_bmp_offsets vector
 *  Will not search at all if the chunk is missing or is already scanned.
 *  @return Pointer to files_stp->tile_bmp_offsets. */
int fscan_search_for_tile_bmps(fscan_files *files_stp);

int fscan_draw_gfx_using_gfx_info(fscan_files *files_stp, const fscan_gfx_info *ginf, gfx_graphic *output);

int fscan_draw_gfx_using_gfx_info_ex(fscan_files *files_stp,
                                     const fscan_gfx_info *ginf,
                                     gfx_graphic *output,
                                     int pos_x,
                                     int pos_y,
                                     int flags);