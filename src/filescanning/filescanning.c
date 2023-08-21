#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "fseeking_helper.h"
#include "filescanning.h"
#include "../helpers/basicdefs.h"
#include "../graphics/gfx.h"
#include "../helpers/binary_parse.h"
#include "../essentials/ptr_map.h"

// ___________________________________________________ STATIC FUNCTION DECLARATIONS ___________________________________________________

static uptr fscan_infilePtrToOffset(u32 infile_ptr, uptr startOffset);
static u32 fscan_offsetToInfilePtr(uptr fileOffset, uptr startOffset);

// part of fscan_init
static inline int _fscan_files_init_open_and_set(const char filename[], FILE * generalFp, size_t fileSize, fscan_file_chunk fchunk[1]);

// _______________________________________________________ FUNCTION DEFINITIONS _______________________________________________________


//  --- part of fscan_files_init ---
/// @return -1 fopen failed (don't forget to close mainFp in client), 0 success, 1 invalid chunk
static int _fscan_files_init_open_and_set(const char filename[], FILE * generalFp, size_t fileSize, fscan_file_chunk fchunk[1]){
    fread_LE_U32( (u32*)&fchunk->size, 1, generalFp);
    fread_LE_U32(&fchunk->offset, 1, generalFp);
    if(!(fchunk->offset && fchunk->size > 32 && fchunk->offset + fchunk->size <= fileSize)) return 1;
    if(!(fchunk->ptrs_fp = fopen(filename, "rb")) 
    || !(fchunk->data_fp = fopen(filename, "rb"))){
        return -1;
    }
    // non-ptr int arithmetics below.  Setting initial stream positions & Entry point offset
    u32 epOffset = fchunk->offset + fchunk->size / 2048 + 4; //< entry point address for ptrs lookup
    fseek(fchunk->data_fp, fchunk->offset, SEEK_SET); 
    fseek(fchunk->ptrs_fp, epOffset, SEEK_SET);
    fread_LE_U32(&fchunk->ep, 1, fchunk->ptrs_fp);
    fchunk->ep = (u32) fscan_infilePtrToOffset(fchunk->ep, fchunk->offset);

    return 0;
}

int fscan_files_init(struct fscan_files * filesStp, const char filename[]){
    FILE * fp = NULL;
    u32 fileChunksCount = 0;
    size_t fileSize = 0;
    int retVal = 0;

    if(gexdev_u32vec_init_capcity(&filesStp->ext_bmp_offsets, 256)) exit(0x1234);

    fp = fopen(filename, "rb");
    if(fp == NULL) return FSCAN_LEVEL_TYPE_FOPEN_ERROR;

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);

    if(fileSize < FILE_MIN_SIZE) return FSCAN_LEVEL_TYPE_FILE_TOO_SMALL;
    //read first value
    rewind(fp);
    if(!fread_LE_U32(&fileChunksCount, 1, fp)) return -3;

    // Check file type
    if(fileChunksCount >= 5 && fileChunksCount <= 32){
        //FILE TYPE: STANDARD LEVEL

        // Tile bitmaps chunk setup
        fseek(fp, 0x28, SEEK_SET); 
        switch(_fscan_files_init_open_and_set(filename, fp, fileSize, &filesStp->tile_chunk)) {
            case -1: fclose(fp); return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 1: retVal |= FSCAN_LEVEL_FLAG_NO_TILES; break; // invalid / non-exsiting chunk
        }
        // Chunk with bitmaps (of backgrounds and objects) setup
        fseek(fp, 8, SEEK_CUR); 
        switch(_fscan_files_init_open_and_set(filename, fp, fileSize, &filesStp->bitmap_chunk)) {
            case -1: fclose(fp); return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 1: retVal |= FSCAN_LEVEL_FLAG_NO_BACKGROUND; break; // invalid / non-exsiting chunk
        }
        // Main chunk setup
        fseek(fp, 8, SEEK_CUR);
        switch(_fscan_files_init_open_and_set(filename, fp, fileSize, &filesStp->main_chunk)) {
            case -1: fclose(fp); return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 1: retVal |= FSCAN_LEVEL_FLAG_NO_MAIN; break; // invalid / non-exsiting chunk
        }
        // Intro chunk setup
        fseek(fp, 8, SEEK_CUR);
        switch(_fscan_files_init_open_and_set(filename, fp, fileSize, &filesStp->intro_chunk)) {
            case -1: fclose(fp); return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 1: retVal |= FSCAN_LEVEL_FLAG_NO_BACKGROUND; break; // invalid / non-exsiting chunk
        }
        // Background chunk setup
        fseek(fp, 8, SEEK_CUR); 
        switch(_fscan_files_init_open_and_set(filename, fp, fileSize, &filesStp->bg_chunk)) {
            case -1: fclose(fp); return FSCAN_LEVEL_TYPE_FOPEN_ERROR;
            case 1: retVal |= FSCAN_LEVEL_FLAG_NO_BACKGROUND; break; // invalid / non-exsiting chunk
        }
    }
    else {
        // FILE TYPE: standalone gfx file
        // TODO: more special files detection
        retVal = 1;
    }

    if(fp != NULL) fclose(fp);
    return retVal;
}


void fscan_files_close(struct fscan_files * filesStp){
    if(filesStp->tile_chunk.ptrs_fp)   {fclose(filesStp->tile_chunk.ptrs_fp); filesStp->tile_chunk.ptrs_fp = NULL;}
    if(filesStp->tile_chunk.data_fp)   {fclose(filesStp->tile_chunk.data_fp); filesStp->tile_chunk.data_fp = NULL;}
    if(filesStp->bitmap_chunk.ptrs_fp) {fclose(filesStp->bitmap_chunk.ptrs_fp); filesStp->bitmap_chunk.ptrs_fp = NULL;}
    if(filesStp->bitmap_chunk.data_fp) {fclose(filesStp->bitmap_chunk.data_fp); filesStp->bitmap_chunk.data_fp = NULL;}
    if(filesStp->main_chunk.ptrs_fp)   {fclose(filesStp->main_chunk.ptrs_fp); filesStp->main_chunk.ptrs_fp = NULL;}
    if(filesStp->main_chunk.data_fp)   {fclose(filesStp->main_chunk.data_fp); filesStp->main_chunk.data_fp = NULL;}
    if(filesStp->intro_chunk.ptrs_fp)  {fclose(filesStp->intro_chunk.ptrs_fp); filesStp->intro_chunk.ptrs_fp = NULL;}
    if(filesStp->intro_chunk.data_fp)  {fclose(filesStp->intro_chunk.data_fp); filesStp->intro_chunk.data_fp = NULL;}
    if(filesStp->bg_chunk.ptrs_fp)     {fclose(filesStp->bg_chunk.ptrs_fp); filesStp->bg_chunk.ptrs_fp = NULL;}
    if(filesStp->bg_chunk.data_fp)     {fclose(filesStp->bg_chunk.data_fp); filesStp->bg_chunk.data_fp = NULL;}
    gexdev_u32vec_close(&filesStp->ext_bmp_offsets);
}


u32 fscan_read_infile_ptr(FILE * fp, u32 chunkOffset, jmp_buf *error_jmp_buf){
    u32 val = 0;
    if(!fread_LE_U32(&val, 1, fp) && error_jmp_buf)
        longjmp(*error_jmp_buf, FSCAN_READ_ERROR_FREAD);

    return (u32)fscan_infilePtrToOffset(val, chunkOffset);
}

size_t fscan_fread(void *dest, size_t size, size_t n, FILE * fp, jmp_buf *error_jmp_buf){
    size_t retval = fread(dest, size, n, fp);
    if(retval < n && error_jmp_buf)
        longjmp(*error_jmp_buf, FSCAN_READ_ERROR_FREAD);
    return retval;
}


int fscan_cb_read_offset_to_vec_2lvls(fscan_file_chunk * chp, gexdev_u32vec *iter, u32 *ivars, void * clientp){
    gexdev_u32vec * vec_arr = clientp; //[2]
    u32 offset = fscan_read_infile_ptr(chp->ptrs_fp, chp->offset, NULL);
    if(!offset) return 0;

    while(vec_arr[0].size <= iter->v[0]){
        gexdev_u32vec_push_back(&vec_arr[0], vec_arr[1].size);
    }

    gexdev_u32vec_push_back(&vec_arr[1], offset);
    return 1;
}

size_t fscan_read_header_and_bitmaps_alloc(fscan_file_chunk *chunkp, fscan_file_chunk *extbmpchunkp, void **header_and_bitmapp,
                                           void **bmp_startpp, const u32 ext_bmp_offsets[], size_t ext_bmp_offsets_size,
                                           unsigned int *bmp_indexp, jmp_buf *errbufp, gexdev_ptr_map *header_bmp_bindsp)
{
    size_t headerSize = 0;
    size_t totalBitmapSize = 0;
    struct gex_gfxheader gfxHeader = {0};
    bool isBmpExtern = false;
    u32 headerOffset = fscan_read_infile_ptr(chunkp->ptrs_fp, chunkp->offset, errbufp);

    // header read
    fseek(chunkp->data_fp, headerOffset, SEEK_SET);
    gex_gfxheader_parsef(chunkp->data_fp, &gfxHeader);

    if((gfxHeader.typeSignature & 0xF0) == 0xC0) isBmpExtern = true;

    fseek(chunkp->data_fp, headerOffset, SEEK_SET);

    headerSize = gfx_read_headers_alloc_aob(chunkp->data_fp, header_and_bitmapp);
    if(!headerSize){
        if(isBmpExtern) (*bmp_indexp)++; // Skip bitmap
        return 0;
    }

    totalBitmapSize = gfxHeader.typeSignature & 4 ? gfx_calc_size_of_sprite(*header_and_bitmapp)
                                                  : gfx_calc_size_of_bitmap(*header_and_bitmapp);

    if(!totalBitmapSize) { free(*header_and_bitmapp); return 0; }
    if(!(*header_and_bitmapp = realloc(*header_and_bitmapp, headerSize + totalBitmapSize))) exit(0xA4C3D);

    *bmp_startpp = *header_and_bitmapp + headerSize;

    if(isBmpExtern){
        if(!extbmpchunkp->ptrs_fp){
            dbg_errlog("dbg_errlog: bitmap supposed to be in file chunk with bitmaps but there is no such chunk\n");
            free(*header_and_bitmapp);
            return 0;
        }
        // bitmap in bitmap file chunk
        void * mapped_bmp = NULL;
        u32 rel_header_offset = headerOffset - chunkp->offset;
        void *bitmap;

        if((mapped_bmp = gexdev_ptr_map_get(header_bmp_bindsp, &rel_header_offset))){
            bitmap = mapped_bmp; // reuse bitmap
        } else {
            size_t written_bmp_bytes = 0;
            if(!(bitmap = malloc(totalBitmapSize))) exit(0xB4C3D); // freed in gexdev_ptr_map_close_all

            for(void * gchunk = *header_and_bitmapp+20; *(u32*)gchunk; gchunk += 8){
                size_t bitmap_part_size = 0;
                u16 gchunkOffset = written_bmp_bytes + 36;
                u16 sizes[2] = {0};

                if(ext_bmp_offsets_size <= *bmp_indexp)
                    longjmp(*errbufp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

                // bitmap sizes check
                fseek(extbmpchunkp->data_fp, ext_bmp_offsets[*bmp_indexp], SEEK_SET);
                if(fread_LE_U16(sizes, 2, extbmpchunkp->data_fp) != 2)
                    longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

                bitmap_part_size = sizes[0] * sizes[1] * 2;

                if(written_bmp_bytes + bitmap_part_size > totalBitmapSize) 
                   longjmp(*errbufp, FSCAN_ERROR_INDEX_OUT_OF_RANGE);

                // read bitmap
                if(fread(bitmap + written_bmp_bytes,1, bitmap_part_size, extbmpchunkp->data_fp) < bitmap_part_size)
                    longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);

                // overwrite chunk data start offset
                aob_read_LE_U16(&gchunkOffset);
                *(u16*)gchunk = gchunkOffset;

                written_bmp_bytes += bitmap_part_size;
                (*bmp_indexp)++;
            }
            gexdev_ptr_map_set(header_bmp_bindsp, &rel_header_offset, bitmap);
        }
        memcpy(*bmp_startpp, bitmap, totalBitmapSize);
    } else {
        // bitmap next to the header
        if(fread(*bmp_startpp,1, totalBitmapSize, chunkp->data_fp) < totalBitmapSize)
            longjmp(*errbufp, FSCAN_READ_ERROR_FREAD);
    }

    return headerSize + totalBitmapSize;
}


/*
static void fscan_files_check_errors_and_eofs(struct fscan_files * filesStp, int mode){
    if(!filesStp->error_jmp_buf) return;
    switch(mode){
        case 0:
        case 1:
            if(feof(filesStp->tilesPtrsFp) || feof(filesStp->gfxPtrsFp))
                longjmp(*filesStp->error_jmp_buf, FSCAN_READ_ERROR_UNEXPECTED_EOF);
            if(ferror(filesStp->tilesPtrsFp) || ferror(filesStp->gfxPtrsFp)) 
                longjmp(*filesStp->error_jmp_buf, FSCAN_READ_ERROR_FERROR);
            if(mode) return;
        case 2:
            if(feof(filesStp->tilesDataFp) || feof(filesStp->gfxDataFp))
                longjmp(*filesStp->error_jmp_buf, FSCAN_READ_ERROR_UNEXPECTED_EOF);
            if(ferror(filesStp->tilesDataFp) || ferror(filesStp->gfxDataFp)) 
                longjmp(*filesStp->error_jmp_buf, FSCAN_READ_ERROR_FERROR);
            break;
    }
}
*/

static uptr fscan_infilePtrToOffset(u32 infile_ptr, uptr startOffset){
    if(infile_ptr == 0) return 0;
    return startOffset + (infile_ptr >> 20) * 0x2000 + (infile_ptr & 0xFFFF) - 1;
}

static u32 fscan_offsetToInfilePtr(uptr offset, uptr startOffset){
    offset -= startOffset;
    return ((offset >> 13) << 20) + (offset & 0x1FFF) + 1;
}
