#include "filescanning_obj_gfx.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"
#include "../essentials/ptr_map.h"
#include "../helpers/binary_parse.h"

struct obj_gfx_scan_pack {
    struct fsmod_files * filesStp;
    void * pass2cb;
    void (*cb)(void * clientp, const void *bitmap, const void *headerAndOpMap,
               const struct gfx_palette *palette, gexdev_u32vec * itervecp);
    gexdev_ptr_map * bmp_headers_binds_mapp;
    gexdev_u32vec * ext_bmp_offsetsp;
    uint ext_bmp_index;
};

static u32 fsmod_cb_bmp_header_binds_compute_index(const void* key){
    return *(const u32 *)key / 32;
}

static int fsmod_cb_flwpat_push_offset(fsmod_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 * internalVars, void * clientp){
    u32 offset = fsmod_read_infile_ptr(fChunk->ptrsFp, fChunk->offset, NULL);
    if(!offset) return 0;
    gexdev_u32vec_push_back(clientp, offset);
    return 1;
}

static int _fsmod_prep_obj_gfx_and_exec_cb(fsmod_file_chunk fChunk[1], gexdev_u32vec * iterVecp, u32 internalVars[INTERNAL_VAR_CNT], void * clientp){
    struct obj_gfx_scan_pack * packp = clientp;
    fsmod_file_chunk * mainChp = &packp->filesStp->mainChunk;
    fsmod_file_chunk * bmpChp = &packp->filesStp->bitmapChunk;
    void * headerData = NULL;
    void * bitmap = NULL;
    size_t bitmapSize = 0, headerSize = 0;
    struct gex_gfxHeader gfxHeader = {0};
    struct gfx_palette pal = {0};
    bool extbmp = false;
    jmp_buf ** errbufpp = &packp->filesStp->error_jmp_buf;
    u32 * bmp_indexp = &packp->ext_bmp_index; //! TO BE MOVED TO FILESSTP

    // error handling
    FSMOD_ERRBUF_CHAIN_ADD(*errbufpp,
        fprintf(stderr, "_fsmod_prep_obj_gfx_and_exec_cb fread error\n");
        if(headerData) free(headerData);
        if(bitmap) free(bitmap);
    );


    u32 headerOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, *errbufpp);
    u32 paletteOffset = fsmod_read_infile_ptr(mainChp->ptrsFp, mainChp->offset, *errbufpp);

    // header read
    fseek(mainChp->dataFp, headerOffset, SEEK_SET);
    gex_gfxHeader_parsef(mainChp->dataFp, &gfxHeader);

    if((gfxHeader.typeSignature & 0xF0) == 0xC0) extbmp = true;
    
    fseek(mainChp->dataFp, headerOffset, SEEK_SET);

    headerSize = gex_gfxHeadersFToAOB(mainChp->dataFp, &headerData);
    if(!headerSize){
        if(extbmp) packp->ext_bmp_index++; // Skip bitmap
        FSMOD_ERRBUF_REVERT(*errbufpp);
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
            bitmapSize = gfxHeader.typeSignature & 4 ? gfx_checkSizeOfSprite(headerData): gfx_checkSizeOfBitmap(headerData);
            bitmap = calloc(bitmapSize,1);
            for(void * gchunk = headerData+20; *(u32*)gchunk; gchunk += 8){
                size_t bitmap_part_size = 0;
                u16 gchunkOffset = written_bmp_bytes + 36;
                u16 sizes[2] = {0};

                if(packp->ext_bmp_offsetsp->size <= *bmp_indexp) 
                    longjmp(**errbufpp, FSMOD_ERROR_INDEX_OUT_OF_RANGE);

                // bitmap sizes check
                fseek(bmpChp->dataFp, packp->ext_bmp_offsetsp->v[*bmp_indexp], SEEK_SET);
                if(fread_LE_U16(sizes, 2, bmpChp->dataFp) != 2)
                    longjmp(**errbufpp, FSMOD_READ_ERROR_FREAD);

                bitmap_part_size = sizes[0] * sizes[1] * 2;

                if(written_bmp_bytes + bitmap_part_size > bitmapSize) 
                    longjmp(**errbufpp, FSMOD_ERROR_INDEX_OUT_OF_RANGE);

                // read bitmap
                if(fread(bitmap + written_bmp_bytes,1, bitmap_part_size, bmpChp->dataFp) < bitmap_part_size)
                    longjmp(**errbufpp, FSMOD_READ_ERROR_FREAD);

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
        bitmapSize = gfxHeader.typeSignature & 4 ? gfx_checkSizeOfSprite(headerData): gfx_checkSizeOfBitmap(headerData);
        bitmap = malloc(bitmapSize);

        if(fread(bitmap,1, bitmapSize, mainChp->dataFp) < bitmapSize)
            longjmp(**errbufpp, FSMOD_READ_ERROR_FREAD);
    }

    // palette parse
    fseek(mainChp->dataFp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->dataFp, &pal);

    // callback call
    packp->cb(packp->pass2cb, bitmap, headerData, &pal, iterVecp);

    //cleanup
    free(headerData);
    if(!extbmp) free(bitmap);

    FSMOD_ERRBUF_REVERT(*errbufpp);
    return 1;
}

void fsmod_obj_gfx_scan(struct fsmod_files * filesStp, void *pass2cb,
                        void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                                const struct gfx_palette *palette, gexdev_u32vec * itervecp))
{
    gexdev_u32vec ext_bmp_offsets = {0};
    gexdev_ptr_map bmp_headers_binds_map = {0};
    struct obj_gfx_scan_pack scan_pack = {filesStp, pass2cb, cb, &bmp_headers_binds_map,&ext_bmp_offsets, 0};
    gexdev_u32vec_init_capcity(&ext_bmp_offsets, 256);
    gexdev_ptr_map_init(&bmp_headers_binds_map, filesStp->mainChunk.size, fsmod_cb_bmp_header_binds_compute_index);

    FSMOD_ERRBUF_CHAIN_ADD(filesStp->error_jmp_buf, 
        gexdev_ptr_map_close_all(&bmp_headers_binds_map);
        gexdev_u32vec_close(&ext_bmp_offsets);
    )

    fsmod_follow_pattern_recur(&filesStp->bitmapChunk, "e[G{[C;]};3]", &ext_bmp_offsets, fsmod_cb_flwpat_push_offset, &filesStp->error_jmp_buf);

    fsmod_follow_pattern_recur(&filesStp->mainChunk, "e+0x20gg [G{ g [G{ [G{ +20 [G{ [G{ +8c };]  };2]  };] };] }+4;]",
                               &scan_pack, _fsmod_prep_obj_gfx_and_exec_cb, &filesStp->error_jmp_buf);

    // cleanup
    gexdev_ptr_map_close_all(&bmp_headers_binds_map);
    gexdev_u32vec_close(&ext_bmp_offsets);
    FSMOD_ERRBUF_REVERT(filesStp->error_jmp_buf);
}