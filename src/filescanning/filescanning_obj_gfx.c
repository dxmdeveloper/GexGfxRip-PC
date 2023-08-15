#include "filescanning_obj_gfx.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../essentials/ptr_map.h"
#include "../helpers/binary_parse.h"

struct obj_gfx_scan_pack {
    struct fscan_files * filesStp;
    void * pass2cb;
    void (*cb)(void * clientp, const void *bitmap, const void *headerAndOpMap,
               const struct gfx_palette *palette, gexdev_u32vec * itervecp);
    gexdev_ptr_map * bmp_headers_binds_mapp;
    gexdev_u32vec * ext_bmp_offsetsp;
    uint ext_bmp_index;
};

static u32 fscan_cb_bmp_header_binds_compute_index(const void* key){
    return *(const u32 *)key / 32;
}

static int fscan_cb_flwpat_push_offset(fscan_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 * internalVars, void * clientp){
    u32 offset = fscan_read_infile_ptr(fChunk->ptrs_fp, fChunk->offset, NULL);
    if(!offset) return 0;
    gexdev_u32vec_push_back(clientp, offset);
    return 1;
}

static int _fscan_prep_obj_gfx_and_exec_cb(fscan_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 internalVars[INTERNAL_VAR_CNT], void * clientp){
    struct obj_gfx_scan_pack * packp = clientp;
    fscan_file_chunk * mainChp = &packp->filesStp->main_chunk;
    fscan_file_chunk * bmpChp = &packp->filesStp->bitmap_chunk;
    void * headerData = NULL;
    void * bitmap = NULL;
    size_t bitmapSize = 0, headerSize = 0;
    struct gex_gfxheader gfxHeader = {0};
    struct gfx_palette pal = {0};
    bool extbmp = false;
    jmp_buf ** errbufpp = &packp->filesStp->error_jmp_buf;
    uint * bmp_indexp = &packp->filesStp->ext_bmp_index; 

    // error handling
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp,
        fprintf(stderr, "_fscan_prep_obj_gfx_and_exec_cb fread error\n");
        if(headerData) free(headerData);
        if(bitmap) free(bitmap);
    );


    u32 headerOffset = fscan_read_infile_ptr(mainChp->ptrs_fp, mainChp->offset, *errbufpp);
    u32 paletteOffset = fscan_read_infile_ptr(mainChp->ptrs_fp, mainChp->offset, *errbufpp);

    // header read
    fseek(mainChp->data_fp, headerOffset, SEEK_SET);
    gex_gfxheader_parsef(mainChp->data_fp, &gfxHeader);

    if((gfxHeader.typeSignature & 0xF0) == 0xC0) extbmp = true;
    
    fseek(mainChp->data_fp, headerOffset, SEEK_SET);

    headerSize = gfx_read_headers_to_aob(mainChp->data_fp, &headerData);
    if(!headerSize){
        if(extbmp) packp->ext_bmp_index++; // Skip bitmap
        FSCAN_ERRBUF_REVERT(errbufpp);
        return 1;
    }


    if(extbmp){
    // bitmap in bitmap file chunk
        void * mapped_bmp = NULL;
        u32 rel_header_offset = headerOffset - mainChp->offset;
        
        // TODO: CREATE SEPARATE FUNCTION
        if((mapped_bmp = gexdev_ptr_map_get(packp->bmp_headers_binds_mapp, &rel_header_offset))){
            bitmap = mapped_bmp; // reuse bitmap
        } else {
            size_t written_bmp_bytes = 0;
            bitmapSize = gfxHeader.typeSignature & 4 ? gfx_calc_size_of_sprite(headerData): gfx_calc_size_of_bitmap(headerData);
            bitmap = calloc(bitmapSize,1);
            for(void * gchunk = headerData+20; *(u32*)gchunk; gchunk += 8){
                size_t bitmap_part_size = 0;
                u16 gchunkOffset = written_bmp_bytes + 36;
                u16 sizes[2] = {0};

                if(packp->ext_bmp_offsetsp->size <= *bmp_indexp) 
                    longjmp(**errbufpp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

                // bitmap sizes check
                fseek(bmpChp->data_fp, packp->ext_bmp_offsetsp->v[*bmp_indexp], SEEK_SET);
                if(fread_LE_U16(sizes, 2, bmpChp->data_fp) != 2)
                    longjmp(**errbufpp, FSCAN_READ_ERROR_FREAD);

                bitmap_part_size = sizes[0] * sizes[1] * 2;

                if(written_bmp_bytes + bitmap_part_size > bitmapSize) 
                    longjmp(**errbufpp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

                // read bitmap
                if(fread(bitmap + written_bmp_bytes,1, bitmap_part_size, bmpChp->data_fp) < bitmap_part_size)
                    longjmp(**errbufpp, FSCAN_READ_ERROR_FREAD);

                // overwrite chunk data start offset
                aob_read_LE_U16(&gchunkOffset);
                *(u16*)gchunk = gchunkOffset;

                written_bmp_bytes += bitmap_part_size;
                (*bmp_indexp)++;
            }
            gexdev_ptr_map_set(packp->bmp_headers_binds_mapp, &rel_header_offset, bitmap);
        }
    } else {
    // bitmap located right after headers
        bitmapSize = gfxHeader.typeSignature & 4 ? gfx_calc_size_of_sprite(headerData): gfx_calc_size_of_bitmap(headerData);
        bitmap = malloc(bitmapSize);

        if(fread(bitmap,1, bitmapSize, mainChp->data_fp) < bitmapSize)
            longjmp(**errbufpp, FSCAN_READ_ERROR_FREAD);
    }

    // palette parse
    fseek(mainChp->data_fp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->data_fp, &pal);

    // callback call
    packp->cb(packp->pass2cb, bitmap, headerData, &pal, iterVecp);

    //cleanup
    free(headerData);
    if(!extbmp) free(bitmap);

    FSCAN_ERRBUF_REVERT(errbufpp);
    return 1;
}

void fscan_obj_gfx_scan(struct fscan_files * filesStp, void *pass2cb,
                        void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                                const struct gfx_palette *palette, gexdev_u32vec * itervecp))
{
    gexdev_ptr_map bmp_headers_binds_map = {0};
    struct obj_gfx_scan_pack scan_pack = {filesStp, pass2cb, cb, &bmp_headers_binds_map,&filesStp->ext_bmp_offsets, 0};
    gexdev_ptr_map_init(&bmp_headers_binds_map, filesStp->main_chunk.size, fscan_cb_bmp_header_binds_compute_index);

    jmp_buf ** errbufpp = &filesStp->error_jmp_buf;
    FSCAN_ERRBUF_CHAIN_ADD(errbufpp, 
        gexdev_ptr_map_close_all(&bmp_headers_binds_map);
        gexdev_u32vec_close(&filesStp->ext_bmp_offsets);
    )

    fscan_follow_pattern_recur(&filesStp->bitmap_chunk, "e[G{[C;]};3]", &filesStp->ext_bmp_offsets, fscan_cb_flwpat_push_offset, errbufpp);

    // TODO: DOCS OF THE PATTERN
    fscan_follow_pattern_recur(&filesStp->main_chunk, "e+0x20gg [G{ g [G{ [G{ +20 [G{ [G{ +8c };]  };2]  };] };] }+4;]",
                               &scan_pack, _fscan_prep_obj_gfx_and_exec_cb, errbufpp);

    filesStp->used_fchunks_arr[2] = true;
    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    FSCAN_ERRBUF_REVERT(errbufpp);
}