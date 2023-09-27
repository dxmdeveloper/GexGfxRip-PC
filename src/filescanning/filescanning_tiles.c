#include "filescanning_tiles.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"
#include "../essentials/ptr_map.h"
#include "../helpers/basicdefs.h"

//  -------------- STATIC DECLARATIONS --------------

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedValue"
struct tile_scan_cb_pack {
    struct fscan_files *files_stp;
    void *pass2cb;
    void (*dest_cb)(void *, const void *, const void *, const struct gfx_palette *, u16, u16);
    gexdev_ptr_map *bmp_headers_binds_map;
    const gexdev_u32vec *tile_bmp_offsets_vecp; // array of 2 vectors

    uint bmp_index[32];
};

/** @brief made as callback for gexdev_ptr_map to shrink size of tile header pointers table */
static u32 prv_fscan_cb_tile_header_binds_compute_index(const void *key);

/** @brief specific use function. Use as fscan_follow_pattern_recur's callback while scanning for tiles. 
  * @param clientp struct tile_scan_cb_pack */
static int prv_fscan_prep_tile_gfx_data_and_exec_cb(fscan_file_chunk fchp[1], gexdev_u32vec *iter_vecp, u32 *ivars, void *clientp);

// ---------------- FUNC DEFINITIONS ----------------

void fscan_tiles_scan(struct fscan_files *files_stp, void *pass2cb,
		      void cb(void *clientp, const void *headerAndOpMap, const void *bitmap, const struct gfx_palette *palette,
			      uint16_t tileGfxId, uint16_t tileAnimFrameI))
{
    struct tile_scan_cb_pack cbpack = { files_stp, pass2cb, cb };
    gexdev_ptr_map bmp_headers_binds_map = { 0 };
    gexdev_u32vec tile_bmp_offsets[2] = { 0 };
    gexdev_ptr_map_init(&bmp_headers_binds_map, files_stp->main_chunk.size / 32, prv_fscan_cb_tile_header_binds_compute_index);
    gexdev_u32vec_init_capcity(&tile_bmp_offsets[0], 16);
    gexdev_u32vec_init_capcity(&tile_bmp_offsets[1], 256);

    if (!bmp_headers_binds_map.mem_regions)
	exit(0xbeef);
    if (!tile_bmp_offsets[0].v || !tile_bmp_offsets[1].v)
	exit(0xbeef);

    cbpack.bmp_headers_binds_map = &bmp_headers_binds_map;
    cbpack.tile_bmp_offsets_vecp = tile_bmp_offsets;

    // ---------------------- error handling ----------------------
    jmp_buf **errbufpp = &files_stp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, gexdev_u32vec_close(&tile_bmp_offsets[0]); gexdev_u32vec_close(&tile_bmp_offsets[1]);
			   gexdev_ptr_map_close_all(&bmp_headers_binds_map);)
    // -----------------------------------------------------------

    fscan_follow_pattern_recur(&files_stp->tile_chunk, "e[G{[C;]};]", &tile_bmp_offsets, fscan_cb_read_offset_to_vec_2lvls, errbufpp);

    fscan_follow_pattern_recur(&files_stp->main_chunk, "e+0x28g[G{+4[r($0,1,U32) C ;]};]", &cbpack,
			       prv_fscan_prep_tile_gfx_data_and_exec_cb, errbufpp);

    // ! NEW IMPLEMENTATION OF ABOVE IN PROGRESS
    {
	fscan_file_chunk *mchp = &files_stp->main_chunk;

	fseek(mchp->ptrs_fp, mchp->ep + 0x28, SEEK_SET);
	fscan_read_infile_ptr(mchp->ptrs_fp, mchp->offset, *errbufpp);

	u32 block[64] = { 0 };
	for (uint i = 0; i < 64; i++)
	    if ((block[i] = fscan_read_infile_ptr(mchp->ptrs_fp, mchp->offset, *errbufpp)))
		break;

	for (uint i = 0; i < 64 && block[i]; i++) {
	    u32 gfxid = 0;
	    if (!fscan_read_gexptr_and_follow(mchp, 4, *errbufpp))
		continue;
	    fread_LE_U32(&gfxid, 1, mchp->ptrs_fp);
	    /*  call prep function */
	}
    }

    // anim tiles
    fscan_follow_pattern_recur(&files_stp->main_chunk,
			       "e+0x28g[G{       " /* go to tile gfx blocks                                               */
			       "   g                     " /* follow first pointer in a tile gfx block                            */
			       "   [                     " /* open null-terminated do while loop and reset counter                */
			       "       +4 r($0,1,u16) -6 " /* move file cursor by 4, read tile gfx id to first internal var, back */
			       "       G{[C;]}           " /* follow pointer and call callback until it returns non-zero value    */
			       "       +16               " /* go to next animation                                                */
			       "   ;]                    "
			       "};]                      ",
			       &cbpack, prv_fscan_prep_tile_gfx_data_and_exec_cb, errbufpp);

    files_stp->used_fchunks_arr[1] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    gexdev_u32vec_close(&tile_bmp_offsets[0]);
    gexdev_u32vec_close(&tile_bmp_offsets[1]);
    FSCAN_ERRBUF_REVERT(errbufpp);
}

static int prv_fscan_prep_tile_gfx_data_and_exec_cb(fscan_file_chunk fchp[1], gexdev_u32vec *iter_vecp, u32 *ivars, void *clientp)
{
    struct tile_scan_cb_pack *packp = clientp;
    void *header_and_bmp = NULL;
    void *bmpp = NULL;
    size_t gfx_size = 0;
    struct gfx_palette pal = { 0 };
    fscan_file_chunk *main_chp = &packp->files_stp->main_chunk;
    fscan_file_chunk *tile_chp = &packp->files_stp->tile_chunk;
    jmp_buf **errbufpp = &packp->files_stp->error_jmp_buf;
    u32 tile_gfx_id = 0;
    u16 tile_anim_id = 0;
    uint block_ind = iter_vecp->v[0];

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, fprintf(stderr, "prv_fscan_prep_tile_gfx_data_and_exec_cb fread error\n");
			   if (header_and_bmp) free(header_and_bmp);)

    tile_gfx_id = ivars[0];
    if (tile_gfx_id == 0xFFFFFFFF) {
	FSCAN_ERRBUF_REVERT(errbufpp);
	return 0;
    } // end of tile list

    if (iter_vecp->size > 2) {
	tile_anim_id = iter_vecp->v[2] + 1;
    }

    if (!fscan_read_infile_ptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp)) {
	FSCAN_ERRBUF_REVERT(errbufpp);
	return 0;
    }
    fseek(main_chp->ptrs_fp, -4, SEEK_CUR);

    gfx_size = fscan_read_header_and_bitmaps_alloc(main_chp, tile_chp, &header_and_bmp, &bmpp,
						   &packp->tile_bmp_offsets_vecp[1].v[packp->tile_bmp_offsets_vecp[0].v[block_ind]],
						   packp->tile_bmp_offsets_vecp[1].size, &packp->bmp_index[block_ind], *errbufpp,
						   packp->bmp_headers_binds_map, true);
    if (!gfx_size) {
	FSCAN_ERRBUF_REVERT(errbufpp);
	return 1;
    }

    // palette read
    u32 paletteOffset = fscan_read_infile_ptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp);
    fseek(main_chp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(main_chp->data_fp, &pal);

    // CALLING ONFOUND CALLBACK
    packp->dest_cb(packp->pass2cb, header_and_bmp, bmpp, &pal, (u16)tile_gfx_id, tile_anim_id);

    // omit 4 bytes (some flags, but only semi transparency affects tile graphics as far as I know)
    fseek(main_chp->ptrs_fp, 4, SEEK_CUR);

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
