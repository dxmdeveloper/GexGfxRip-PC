#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "filescanning.h"
#include "../helpers/binary_parse.h"

// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr prv_fscan_gexptr_to_offset(u32 gexptr, uptr start_offset);
static u32 prv_fscan_offset_to_gexptr(uptr offset, uptr file_start_offset);

// part of fscan_init
static inline int prv_fscan_files_init_open_and_set(const char filename[], FILE *general_fp, size_t fsize, fscan_file_chunk fchunk[1]);

// _______________________________________________________ FUNCTION DEFINITIONS _______________________________________________________

/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static inline int prv_fscan_files_init_open_and_set(const char filename[], FILE *general_fp, size_t fsize, fscan_file_chunk fchunk[1])
{
    fread_LE_U32((u32 *)&fchunk->size, 1, general_fp);
    fread_LE_U32(&fchunk->offset, 1, general_fp);
    if (!(fchunk->offset && fchunk->size > 32 && fchunk->offset + fchunk->size <= fsize))
	return 1;
    if (!(fchunk->ptrs_fp = fopen(filename, "rb")) || !(fchunk->data_fp = fopen(filename, "rb"))) {
	return -1;
    }
    // non-ptr int arithmetics below.  Setting initial stream positions & Entry point offset
    u32 epOffset = fchunk->offset + fchunk->size / 2048 + 4; //< entry point address for ptrs lookup
    fseek(fchunk->data_fp, fchunk->offset, SEEK_SET);
    fseek(fchunk->ptrs_fp, epOffset, SEEK_SET);
    fread_LE_U32(&fchunk->ep, 1, fchunk->ptrs_fp);
    fchunk->ep = (u32)prv_fscan_gexptr_to_offset(fchunk->ep, fchunk->offset);

    return 0;
}

int fscan_files_init(struct fscan_files *files_stp, const char filename[])
{
    FILE *fp = NULL;
    u32 fchunkcnt = 0;
    size_t fsize = 0;
    int retval = 0;

    // zeroing members
    for (uint i = 0; i < 6; i++)
	files_stp->used_fchunks_arr[i] = false;
    files_stp->ext_bmp_index = 0;

    if (gexdev_u32vec_init_capcity(&files_stp->ext_bmp_offsets, 256))
	exit(0x1234);

    fp = fopen(filename, "rb");
    if (fp == NULL)
	return FSCAN_LEVEL_TYPE_FOPEN_ERROR;

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);

    if (fsize < FILE_MIN_SIZE)
	return FSCAN_LEVEL_TYPE_FILE_TOO_SMALL;
    //read first value
    rewind(fp);
    if (!fread_LE_U32(&fchunkcnt, 1, fp))
	return -3;

    // Check file type
    if (fchunkcnt >= 5 && fchunkcnt <= 32) {
	//FILE TYPE: STANDARD LEVEL

	// Tile bitmaps chunk setup
	fseek(fp, 0x28, SEEK_SET);
	switch (prv_fscan_files_init_open_and_set(filename, fp, fsize, &files_stp->tile_chunk)) {
	case -1:
	    fclose(fp);
	    return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
	case 1:
	    retval |= FSCAN_LEVEL_FLAG_NO_TILES;
	    break; // invalid / non-exsiting chunk
	}
	// Chunk with bitmaps (of backgrounds and objects) setup
	fseek(fp, 8, SEEK_CUR);
	switch (prv_fscan_files_init_open_and_set(filename, fp, fsize, &files_stp->bitmap_chunk)) {
	case -1:
	    fclose(fp);
	    return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
	case 1:
	    retval |= FSCAN_LEVEL_FLAG_NO_BACKGROUND;
	    break; // invalid / non-exsiting chunk
	}
	// Main chunk setup
	fseek(fp, 8, SEEK_CUR);
	switch (prv_fscan_files_init_open_and_set(filename, fp, fsize, &files_stp->main_chunk)) {
	case -1:
	    fclose(fp);
	    return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
	case 1:
	    retval |= FSCAN_LEVEL_FLAG_NO_MAIN;
	    break; // invalid / non-exsiting chunk
	}
	// Intro chunk setup
	fseek(fp, 8, SEEK_CUR);
	switch (prv_fscan_files_init_open_and_set(filename, fp, fsize, &files_stp->intro_chunk)) {
	case -1:
	    fclose(fp);
	    return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
	case 1:
	    retval |= FSCAN_LEVEL_FLAG_NO_BACKGROUND;
	    break; // invalid / non-exsiting chunk
	}
	// Background chunk setup
	fseek(fp, 8, SEEK_CUR);
	switch (prv_fscan_files_init_open_and_set(filename, fp, fsize, &files_stp->bg_chunk)) {
	case -1:
	    fclose(fp);
	    return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
	case 1:
	    retval |= FSCAN_LEVEL_FLAG_NO_BACKGROUND;
	    break; // invalid / non-exsiting chunk
	}
    } else {
	// FILE TYPE: standalone gfx file
	// TODO: more special files detection
	retval = 1;
    }

    if (fp != NULL)
	fclose(fp);
    return retval;
}

void fscan_files_close(struct fscan_files *files_stp)
{
    if (files_stp->tile_chunk.ptrs_fp) {
	fclose(files_stp->tile_chunk.ptrs_fp);
	files_stp->tile_chunk.ptrs_fp = NULL;
    }
    if (files_stp->tile_chunk.data_fp) {
	fclose(files_stp->tile_chunk.data_fp);
	files_stp->tile_chunk.data_fp = NULL;
    }
    if (files_stp->bitmap_chunk.ptrs_fp) {
	fclose(files_stp->bitmap_chunk.ptrs_fp);
	files_stp->bitmap_chunk.ptrs_fp = NULL;
    }
    if (files_stp->bitmap_chunk.data_fp) {
	fclose(files_stp->bitmap_chunk.data_fp);
	files_stp->bitmap_chunk.data_fp = NULL;
    }
    if (files_stp->main_chunk.ptrs_fp) {
	fclose(files_stp->main_chunk.ptrs_fp);
	files_stp->main_chunk.ptrs_fp = NULL;
    }
    if (files_stp->main_chunk.data_fp) {
	fclose(files_stp->main_chunk.data_fp);
	files_stp->main_chunk.data_fp = NULL;
    }
    if (files_stp->intro_chunk.ptrs_fp) {
	fclose(files_stp->intro_chunk.ptrs_fp);
	files_stp->intro_chunk.ptrs_fp = NULL;
    }
    if (files_stp->intro_chunk.data_fp) {
	fclose(files_stp->intro_chunk.data_fp);
	files_stp->intro_chunk.data_fp = NULL;
    }
    if (files_stp->bg_chunk.ptrs_fp) {
	fclose(files_stp->bg_chunk.ptrs_fp);
	files_stp->bg_chunk.ptrs_fp = NULL;
    }
    if (files_stp->bg_chunk.data_fp) {
	fclose(files_stp->bg_chunk.data_fp);
	files_stp->bg_chunk.data_fp = NULL;
    }
    gexdev_u32vec_close(&files_stp->ext_bmp_offsets);
}

u32 fscan_read_infile_ptr(FILE *fp, u32 chunk_offset, jmp_buf *error_jmp_buf)
{
    u32 val = 0;
    if (!fread_LE_U32(&val, 1, fp) && error_jmp_buf)
	longjmp(*error_jmp_buf, FSCAN_READ_ERROR_FREAD);

    return (u32)prv_fscan_gexptr_to_offset(val, chunk_offset);
}

size_t fscan_fread(void *dest, size_t size, size_t n, FILE *fp, jmp_buf *error_jmp_buf)
{
    size_t retval = fread(dest, size, n, fp);
    if (retval < n && error_jmp_buf)
	longjmp(*error_jmp_buf, FSCAN_READ_ERROR_FREAD);
    return retval;
}

// TODO: REMOVE THIS FUNCTION
int fscan_cb_read_offset_to_vec_2lvls(fscan_file_chunk *chp, gexdev_u32vec *iter, u32 *ivars, void *clientp)
{
    gexdev_u32vec *vec_arr = clientp; //[2]
    u32 offset = fscan_read_infile_ptr(chp->ptrs_fp, chp->offset, NULL);
    if (!offset)
	return 0;

    while (vec_arr[0].size <= iter->v[0]) {
	gexdev_u32vec_push_back(&vec_arr[0], vec_arr[1].size);
    }

    gexdev_u32vec_push_back(&vec_arr[1], offset);
    return 1;
}

size_t fscan_read_header_and_bitmaps_alloc(fscan_file_chunk *fchp, fscan_file_chunk *extbmpchunkp, void **header_and_bitmapp,
					   void **bmp_startpp, const u32 ext_bmp_offsets[], size_t ext_bmp_offsets_size,
					   unsigned int *bmp_indexp, jmp_buf(*errbufp), gexdev_ptr_map *header_bmp_bindsp, bool is_tile)
{
    size_t header_size = 0;
    size_t total_bmp_size = 0;
    struct gex_gfxheader gfxheader = { 0 };
    bool is_bmp_extern = false;
    u32 header_offset = fscan_read_infile_ptr(fchp->ptrs_fp, fchp->offset, errbufp);

    // header read
    fseek(fchp->data_fp, header_offset, SEEK_SET);
    gex_gfxheader_parsef(fchp->data_fp, &gfxheader);

    if ((gfxheader.type_signature & 0xF0) == 0xC0) {
	is_bmp_extern = true;
	if (extbmpchunkp->ptrs_fp == NULL) {
	    fprintf(stderr, "error: fscan_read_header_and_bitmaps_alloc extbmpchunkp param does not point a valid file chunk\n");
	    return 0;
	}
    }

    fseek(fchp->data_fp, header_offset, SEEK_SET);

    header_size = gfx_read_headers_alloc_aob(fchp->data_fp, header_and_bitmapp);
    if (!header_size) {
	if (is_bmp_extern && is_tile) {
	    (*bmp_indexp)++; // Skip bitmap
	}
	return 0;
    }

    total_bmp_size = gfxheader.type_signature & 4 ? gfx_calc_size_of_sprite(*header_and_bitmapp) :
						    gfx_calc_size_of_bitmap(*header_and_bitmapp);

    if (!total_bmp_size) {
	free(*header_and_bitmapp);
	return 0;
    }
    if (!(*header_and_bitmapp = realloc(*header_and_bitmapp, header_size + total_bmp_size)))
	exit(0xA4C3D);

    *bmp_startpp = *header_and_bitmapp + header_size;

    if (is_bmp_extern) {
	// bitmap in bitmap file chunk
	u32 rel_header_off = header_offset - fchp->offset;
	u8 *bmp_from_map = NULL;

	if ((bmp_from_map = gexdev_ptr_map_get(header_bmp_bindsp, &rel_header_off))) {
	} else {
	    size_t written_bmp_bytes = 0;
	    if (!(bmp_from_map = malloc(total_bmp_size + header_size)))
		exit(0xB4C3D); // freed in gexdev_ptr_map_close_all

	    for (void *gchunk = *header_and_bitmapp + 20; *(u32 *)gchunk; gchunk += 8) {
		size_t bmp_part_size = 0;
		u16 gchunk_off = written_bmp_bytes + 36;
		u16 sizes[2] = { 0 };

		if (ext_bmp_offsets_size <= *bmp_indexp)
		    longjmp(*errbufp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

		// bitmap sizes check
		fseek(extbmpchunkp->data_fp, ext_bmp_offsets[*bmp_indexp], SEEK_SET);
		if (fread_LE_U16(sizes, 2, extbmpchunkp->data_fp) != 2)
		    longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

		bmp_part_size = sizes[0] * sizes[1] * 2;

		if (written_bmp_bytes + bmp_part_size > total_bmp_size)
		    longjmp(*errbufp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

		// read bitmap
		if (fread(bmp_from_map + header_size + written_bmp_bytes, 1, bmp_part_size, extbmpchunkp->data_fp) < bmp_part_size)
		    longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

		// overwrite chunk data start offset
		aob_read_LE_U16(&gchunk_off);
		*(u16 *)gchunk = gchunk_off;

		written_bmp_bytes += bmp_part_size;
		(*bmp_indexp)++;
	    }
	    //! DEBUG. NOTE: MAY BE WRONG
	    if (written_bmp_bytes < total_bmp_size) {
		printf("DEBUG INFO: read bitmap bytes and expected bitmap size difference: %lu\n", total_bmp_size - written_bmp_bytes);
	    }
	    memcpy(bmp_from_map, *header_and_bitmapp, header_size); // copy header before mapping
	    gexdev_ptr_map_set(header_bmp_bindsp, &rel_header_off, bmp_from_map);
	}
	memcpy(*header_and_bitmapp, bmp_from_map, total_bmp_size + header_size);
    } else {
	// bitmap next to the header
	if (fread(*bmp_startpp, 1, total_bmp_size, fchp->data_fp) < total_bmp_size)
	    longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);
    }

    return header_size + total_bmp_size;
}
uint32_t fscan_read_gexptr_and_follow(fscan_file_chunk *fchp, int addoff, jmp_buf(*errbufp))
{
    u32 gexptr = fscan_read_infile_ptr(fchp->ptrs_fp, fchp->offset, errbufp);
    if (!gexptr || gexptr >= fchp->offset + fchp->size)
	return 0;
    fseek(fchp->ptrs_fp, gexptr + addoff, SEEK_SET);
    return gexptr;
}

size_t fscan_read_gexptr_null_term_arr(fscan_file_chunk *fchp, uint32_t dest[], size_t dest_size, jmp_buf(*errbufp))
{
    for (uint i = 0; i < dest_size-1; i++)
	if (!(dest[i] = fscan_read_infile_ptr(fchp->ptrs_fp, fchp->offset, errbufp))
	    || dest[i] >= fchp->size + fchp->offset - 4) {
	    dest[i] = 0;
	    return i;
	}
    dest[dest_size-2] = 0;
    return dest_size-1;
}

static uptr prv_fscan_gexptr_to_offset(u32 gexptr, uptr start_offset)
{
    if (gexptr == 0)
	return 0;
    return start_offset + (gexptr >> 20) * 0x2000 + (gexptr & 0xFFFF) - 1;
}

static u32 prv_fscan_offset_to_gexptr(uptr offset, uptr file_start_offset)
{
    offset -= file_start_offset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}
