#include "filescanning_tiles.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"
#include "../essentials/ptr_map.h"
#include "../helpers/basicdefs.h"

//  -------------- STATIC DECLARATIONS --------------

/** @brief made as callback for gexdev_ptr_map to shrink size of tile header pointers table */
static u32 prv_fscan_cb_tile_header_binds_compute_index(const void *key);

/** @brief specific use function. Use as fscan_follow_pattern_recur's callback while scanning for tiles. 
  * @param tile_bmp_offsets_vecp array of 2 u32 vectors. First vector points where offsets of each block starts in second vector
  * @param bmp_index zero initialized array of 32 uints. Function keeps information from previous calls */
static int prv_fscan_prep_tile_gfx_data_and_exec_cb(struct fscan_files files_stp[1], u32 tile_gfx_id, uint tile_anim_frame, uint block_ind,
						    gexdev_ptr_map *bmp_headers_binds_map, const gexdev_u32vec tile_bmp_offsets_vecp[2],
						    uint bmp_index[32], void *pass2cb,
						    void cb(void *, const void *, const void *, const struct gfx_palette *, u16, u16));

// ---------------- FUNC DEFINITIONS ----------------

void fscan_tiles_scan(struct fscan_files *files_stp, void *pass2cb,
		      void cb(void *clientp, const void *headerAndOpMap, const void *bitmap, const struct gfx_palette *palette,
			      uint16_t tileGfxId, uint16_t tileAnimFrameI))
{
    gexdev_ptr_map bmp_headers_binds_map = { 0 };
    gexdev_u32vec tile_bmp_offsets[2] = { 0 };
    gexdev_ptr_map_init(&bmp_headers_binds_map, files_stp->main_chunk.size / 32, prv_fscan_cb_tile_header_binds_compute_index);
    gexdev_u32vec_init_capcity(&tile_bmp_offsets[0], 16);
    gexdev_u32vec_init_capcity(&tile_bmp_offsets[1], 256);

    if (!bmp_headers_binds_map.mem_regions)
	exit(0xbeef);
    if (!tile_bmp_offsets[0].v || !tile_bmp_offsets[1].v)
	exit(0xbeef);

    // ---------------------- error handling ----------------------
    jmp_buf **errbufpp = &files_stp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, gexdev_u32vec_close(&tile_bmp_offsets[0]); gexdev_u32vec_close(&tile_bmp_offsets[1]);
			   gexdev_ptr_map_close_all(&bmp_headers_binds_map);)
    // -----------------------------------------------------------

    // TODO: REMOVE THE HELPER HERE TOO
    fscan_follow_pattern_recur(&files_stp->tile_chunk, "e[G{[C;]};]", &tile_bmp_offsets, fscan_cb_read_offset_to_vec_2lvls, errbufpp);

    {
	uint bmp_iters[32] = { 0 };
	fscan_file_chunk *mchp = &files_stp->main_chunk;

	fseek(mchp->ptrs_fp, mchp->ep + 0x28, SEEK_SET);
	fscan_read_gexptr_and_follow(mchp, 0, *errbufpp);

	// read offsets of tile gfx blocks
	u32 block[32] = { 0 };
	for (uint i = 0; i < 32; i++)
	    if ((block[i] = fscan_read_infile_ptr(mchp->ptrs_fp, mchp->offset, *errbufpp)))
		break;

	// follow the read offsets, prepare tile graphics and call the callback passed to current function.
	for (uint i = 0; i < 32 && block[i]; i++) {
	    u32 gfxid = 0;
	    if (block[i] >= mchp->offset + mchp->size + 20)
		continue;

	    fseek(mchp->ptrs_fp, block[i] + 4, SEEK_SET);
	    while (true) {
		fread_LE_U32(&gfxid, 1, mchp->ptrs_fp);
		if (!prv_fscan_prep_tile_gfx_data_and_exec_cb(files_stp, gfxid, 0, i, &bmp_headers_binds_map, tile_bmp_offsets, bmp_iters,
							      pass2cb, cb))
		    break;
	    }
	}
    }

    // anim tiles
    // TODO: IMPLEMENT NEW SOLUTION
    //fscan_follow_pattern_recur(&files_stp->main_chunk, 0, &cbpack, prv_fscan_prep_tile_gfx_data_and_exec_cb, 0);

    files_stp->used_fchunks_arr[1] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    gexdev_u32vec_close(&tile_bmp_offsets[0]);
    gexdev_u32vec_close(&tile_bmp_offsets[1]);
    FSCAN_ERRBUF_REVERT(errbufpp);
}

static int prv_fscan_prep_tile_gfx_data_and_exec_cb(struct fscan_files files_stp[1], u32 tile_gfx_id, uint tile_anim_frame, uint block_ind,
						    gexdev_ptr_map *bmp_headers_binds_map, const gexdev_u32vec tile_bmp_offsets_vecp[2],
						    uint bmp_index[32], void *pass2cb,
						    void cb(void *, const void *, const void *, const struct gfx_palette *, u16, u16))
{
    void *header_and_bmp = NULL;
    void *bmpp = NULL;
    size_t gfx_size = 0;
    struct gfx_palette pal = { 0 };
    fscan_file_chunk *mchp = &files_stp->main_chunk;
    fscan_file_chunk *tchp = &files_stp->tile_chunk;
    jmp_buf **errbufpp = &files_stp->error_jmp_buf;

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, {
	fprintf(stderr, "prv_fscan_prep_tile_gfx_data_and_exec_cb error\n");
	if (header_and_bmp)
	    free(header_and_bmp);
    })

    if (tile_gfx_id == 0xFFFFFFFF || block_ind >= 32) {
	// end of tile list
	FSCAN_ERRBUF_REVERT(errbufpp);
	return 0;
    }

    if (!fscan_read_infile_ptr(mchp->ptrs_fp, mchp->offset, *errbufpp)) {
	FSCAN_ERRBUF_REVERT(errbufpp);
	return 0;
    }
    fseek(mchp->ptrs_fp, -4, SEEK_CUR);

    if (tile_bmp_offsets_vecp[0].size <= block_ind || tile_bmp_offsets_vecp[1].size <= tile_bmp_offsets_vecp[0].v[block_ind])
	longjmp(**errbufpp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

    gfx_size = fscan_read_header_and_bitmaps_alloc(mchp, tchp, &header_and_bmp, &bmpp,
						   &tile_bmp_offsets_vecp[1].v[tile_bmp_offsets_vecp[0].v[block_ind]],
						   tile_bmp_offsets_vecp[1].size, &bmp_index[block_ind], *errbufpp, bmp_headers_binds_map,
						   true);
    if (!gfx_size) {
	FSCAN_ERRBUF_REVERT(errbufpp);
	return 1;
    }

    // palette read
    u32 paletteOffset = fscan_read_infile_ptr(mchp->ptrs_fp, mchp->offset, *errbufpp);
    fseek(mchp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mchp->data_fp, &pal);

    // CALLING ON FOUND CALLBACK
    cb(pass2cb, header_and_bmp, bmpp, &pal, (u16)tile_gfx_id, tile_anim_frame);

    // omit 4 bytes (some flags, but only semi transparency affects tile graphics as far as I know)
    fseek(mchp->ptrs_fp, 4, SEEK_CUR);

    // cleanup
    free(header_and_bmp);
    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

static u32 prv_fscan_cb_tile_header_binds_compute_index(const void *key)
{
    const u32 *u32key = key;
    return *u32key / 32;
}
