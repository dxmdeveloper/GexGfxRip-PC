#include "filescanning_obj_gfx_and_bg.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../essentials/ptr_map.h"
#include "../helpers/binary_parse.h"

// TODO: DOCS OF THE PATTERNS
#define FSCAN_OBJ_GFX_FLW_PATTERN "e+0x20gg [G{ g [G{ [G{ +24 g [G{ c };]   };] };] }+4;]"
#define FSCAN_BACKGROUND_FLW_PATTERN "eg+24[G{ +48 [G{ +4 ggg   [G{+24g[G{ c };]};]   };] };]"

struct obj_gfx_scan_pack {
    struct fscan_files *files_stp;
    void *pass2cb;
    void (*cb)(void *clientp, const void *bitmap, const void *headerAndOpMap, const struct gfx_palette *palette, u32 iters[4],
	       struct gfx_properties *);
    gexdev_ptr_map *bmp_headers_binds_mapp;
    gexdev_u32vec *ext_bmp_offsetsp;
    uint ext_bmp_index;
};

// _________________________________ static function declarations _________________________________
static u32 p_cb_bmp_header_binds_compute_index(const void *key);
static int p_cb_flwpat_push_offset(fscan_file_chunk fChunk[1], gexdev_u32vec *iterVecp, u32 *internalVars, void *clientp);
static int p_cb_ext_bmp_index_count(fscan_file_chunk fChunk[1], gexdev_u32vec *iterVecp, u32 *iv, void *clientp);
static int p_cb_prep_obj_gfx_and_exec_cb(fscan_file_chunk fchunk[1], gexdev_u32vec *iter_vecp, u32 *iv, void *clientp);
inline static void
p_scan_chunk_for_obj_gfx(struct fscan_files *files_stp, fscan_file_chunk *fchp, bool dont_prep_gfx_data, char *flw_pattern, void *pass2cb,
			 void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *, struct gfx_properties *));

// _____________________________________ function definitions _____________________________________

void fscan_obj_gfx_scan(struct fscan_files *files_stp, void *pass2cb,
			void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *, struct gfx_properties *))
{
    if (files_stp->option_verbose)
	printf("------------- object scan -------------\n");

    if (files_stp->used_fchunks_arr[2])
	files_stp->ext_bmp_index = 0;

    p_scan_chunk_for_obj_gfx(files_stp, &files_stp->main_chunk, false, FSCAN_OBJ_GFX_FLW_PATTERN, pass2cb, cb);

    files_stp->used_fchunks_arr[3] = true;
}

void fscan_intro_obj_gfx_scan(struct fscan_files *files_stp, void *pass2cb,
			      void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *, struct gfx_properties *))
{
    if (files_stp->option_verbose)
	printf("----------- intro object scan ----------\n");

    if (files_stp->used_fchunks_arr[4] || files_stp->used_fchunks_arr[5])
	files_stp->ext_bmp_index = 0;

    if (!files_stp->ext_bmp_index && files_stp->main_chunk.ptrs_fp) {
	p_scan_chunk_for_obj_gfx(files_stp, &files_stp->main_chunk, true, FSCAN_OBJ_GFX_FLW_PATTERN, NULL, NULL);

	files_stp->used_fchunks_arr[3] = true;
    }

    p_scan_chunk_for_obj_gfx(files_stp, &files_stp->intro_chunk, false, FSCAN_OBJ_GFX_FLW_PATTERN, pass2cb, cb);
    files_stp->used_fchunks_arr[4] = true;
}

void fscan_background_scan(struct fscan_files *files_stp, void *pass2cb,
			   void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *, struct gfx_properties *))
{
    if (files_stp->option_verbose)
	printf("------------ background scan ------------\n");

    if (files_stp->used_fchunks_arr[5]) {
	files_stp->ext_bmp_index = 0;
	files_stp->used_fchunks_arr[3] = false;
	files_stp->used_fchunks_arr[4] = false;
    }
    // Scan main and intro chunks before if not scanned yet
    if (!files_stp->used_fchunks_arr[3] && files_stp->main_chunk.ptrs_fp) {
	p_scan_chunk_for_obj_gfx(files_stp, &files_stp->main_chunk, true, FSCAN_OBJ_GFX_FLW_PATTERN, NULL, NULL);
	files_stp->used_fchunks_arr[3] = true;
    }
    if (!files_stp->used_fchunks_arr[4] && files_stp->intro_chunk.ptrs_fp) {
	p_scan_chunk_for_obj_gfx(files_stp, &files_stp->intro_chunk, true, FSCAN_OBJ_GFX_FLW_PATTERN, NULL, NULL);
	files_stp->used_fchunks_arr[4] = true;
    }

    // Scan background file chunk
    p_scan_chunk_for_obj_gfx(files_stp, &files_stp->bg_chunk, false, FSCAN_BACKGROUND_FLW_PATTERN, pass2cb, cb);
    files_stp->used_fchunks_arr[5] = true;
}

static u32 p_cb_bmp_header_binds_compute_index(const void *key)
{
    return *(const u32 *)key / 32;
}

static int p_cb_flwpat_push_offset(fscan_file_chunk fChunk[1], gexdev_u32vec *iterVecp, u32 *internalVars, void *clientp)
{
    u32 offset = fscan_read_gexptr(fChunk->ptrs_fp, fChunk->offset, NULL);
    u16 wh[2] = { 0 };
    if (!offset)
	return 0;
    fseek(fChunk->data_fp, offset, SEEK_SET);
    fread_LE_U16(wh, 2, fChunk->data_fp);
    if (wh[0] && wh[1])
	gexdev_u32vec_push_back(clientp, offset);
    return 1;
}

// add return 0 (???)
static int p_cb_ext_bmp_index_count(fscan_file_chunk fChunk[1], gexdev_u32vec *iterVecp, u32 *iv, void *clientp)
{
    struct obj_gfx_scan_pack *packp = clientp;
    u32 headerOffset = fscan_read_gexptr(fChunk->ptrs_fp, fChunk->offset, NULL);
    u8 gfxType = 0;
    void *headerData = NULL;
    fseek(fChunk->ptrs_fp, headerOffset + 16, SEEK_SET);
    fread(&gfxType, 1, 1, fChunk->ptrs_fp);
    if ((gfxType & 0xF0) != 0xC0)
	return 1;

    fseek(fChunk->data_fp, headerOffset, SEEK_SET);
    if (!gfx_read_headers_alloc_aob(fChunk->data_fp, &headerData)) {
	packp->files_stp->ext_bmp_index++;
	return 1;
    }

    u32 relOffset = headerOffset - fChunk->offset;
    if (gexdev_ptr_map_get(packp->bmp_headers_binds_mapp, &relOffset)) {
	if (headerData)
	    free(headerData);
	return 1;
    }

    for (void *gch = headerData + 20; *(u32 *)gch; gch += 8) {
	packp->files_stp->ext_bmp_index++;
    }

    gexdev_ptr_map_set(packp->bmp_headers_binds_mapp, &relOffset, malloc(1));
    if (headerData)
	free(headerData);
    return 1;
}

static int p_cb_prep_obj_gfx_and_exec_cb(fscan_file_chunk fchunk[1], gexdev_u32vec *iter_vecp, u32 *iv, void *clientp)
{
    struct obj_gfx_scan_pack *packp = clientp;
    fscan_file_chunk *main_chp = fchunk;
    fscan_file_chunk *bmp_chp = &packp->files_stp->bitmap_chunk;
    jmp_buf **errbufpp = &packp->files_stp->error_jmp_buf;

    void *header_and_bmp = NULL;
    void *bmpp = NULL;
    size_t gfx_size = 0;
    u32 gfx_flags = 0;
    struct gfx_palette pal = { 0 };
    struct gfx_properties gfx_props = { 0 };

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, fprintf(stderr, "prev_fscan_prep_obj_gfx_and_exec_cb fread error\n");
			   if (header_and_bmp) free(header_and_bmp);)

    // graphic properties read
    fread_LE_U16(&gfx_props.pos_y, 1, main_chp->ptrs_fp);
    fread_LE_U16(&gfx_props.pos_x, 1, main_chp->ptrs_fp);
    fread_LE_U32(&gfx_flags, 1, main_chp->ptrs_fp);
    gfx_props.is_flipped_vertically = gfx_flags & (1 << 7);
    gfx_props.is_flipped_horizontally = gfx_flags & (1 << 6);

    // print information
    if (packp->files_stp->option_verbose) {
	u32 header_off = fscan_read_gexptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp);
	u32 pal_off = fscan_read_gexptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp);
	char type;
	if (!header_off)
	    return 1;
	fseek(fchunk->data_fp, header_off + 16, SEEK_SET);
	if (!fread(&type, 1, 1, fchunk->data_fp))
	    return 1;
	printf("found image at: %lx  {\"type\": %0hhX, \"header offset\": %x, \"palette offset\": %x", ftell(fchunk->ptrs_fp) - 16, type,
	       header_off, pal_off);
	if ((type & 0xF0) == 0xC0) {
	    printf(",\"ext_bmp_index\": %u", packp->files_stp->ext_bmp_index);
	}
	printf("} iterVec: [");
	for (size_t i = 0; i < iter_vecp->size - 1; i++) {
	    printf("%u, ", iter_vecp->v[i]);
	}
	printf("%u]\n", iter_vecp->v[iter_vecp->size - 1]);
	fseek(main_chp->ptrs_fp, -8, SEEK_CUR);
    }

    gfx_size = fscan_read_header_and_bitmaps_alloc(main_chp, bmp_chp, &header_and_bmp, &bmpp, packp->ext_bmp_offsetsp->v,
						   packp->ext_bmp_offsetsp->size, &packp->files_stp->ext_bmp_index, *errbufpp,
						   packp->bmp_headers_binds_mapp);
    if (gfx_size == 0) {
	FSCAN_ERRBUF_REVERT(errbufpp);
	return 1;
    }

    // palette parse
    u32 paletteOffset = fscan_read_gexptr(main_chp->ptrs_fp, main_chp->offset, *errbufpp);

    fseek(main_chp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(main_chp->data_fp, &pal);

    // finish reading properties
    fread_LE_U32(&gfx_flags, 1, main_chp->ptrs_fp);
    gfx_flags = gfx_flags >> 16;
    gfx_props.is_semi_transparent = gfx_flags & (1 << 15);
    // TODO: Investigate for more properties

    // callback call
    packp->cb(packp->pass2cb, header_and_bmp, bmpp, &pal, iter_vecp->v, &gfx_props);

    //cleanup
    free(header_and_bmp);

    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

inline static void
p_scan_chunk_for_obj_gfx(struct fscan_files *files_stp, fscan_file_chunk *fchp, bool dont_prep_gfx_data, char *flw_pattern, void *pass2cb,
			 void cb(void *, const void *, const void *, const struct gfx_palette *, u32 *, struct gfx_properties *))
{
    gexdev_ptr_map bmp_headers_binds_map = { 0 };
    struct obj_gfx_scan_pack scan_pack = { files_stp, pass2cb, cb, &bmp_headers_binds_map, &files_stp->ext_bmp_offsets };
    gexdev_ptr_map_init(&bmp_headers_binds_map, fchp->size, p_cb_bmp_header_binds_compute_index);

    jmp_buf **errbufpp = &files_stp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, gexdev_ptr_map_close_all(&bmp_headers_binds_map);)

    if (!files_stp->used_fchunks_arr[2] && files_stp->bitmap_chunk.ptrs_fp)
	fscan_follow_pattern_recur(&files_stp->bitmap_chunk, "e[G{[C;]};6]", &files_stp->ext_bmp_offsets, p_cb_flwpat_push_offset,
				   errbufpp);

    fscan_follow_pattern_recur(fchp, flw_pattern, &scan_pack,
			       (dont_prep_gfx_data ? p_cb_ext_bmp_index_count : p_cb_prep_obj_gfx_and_exec_cb), errbufpp);

    files_stp->used_fchunks_arr[2] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    FSCAN_ERRBUF_REVERT(errbufpp);
}