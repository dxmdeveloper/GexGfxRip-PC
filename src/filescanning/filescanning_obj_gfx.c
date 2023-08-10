#include "filescanning_obj_gfx.h"
#include "filescanning.h"
#include "fseeking_helper.h"
#include "../helpers/basicdefs.h"

struct obj_gfx_scan_pack {
    struct fsmod_files * filesStp;
    void * pass2cb;
    void (*cb)(void * clientp, const void *bitmap, const void *headerAndOpMap,
               const struct gfx_palette *palette, gexdev_u32vec * itervecp);
};

static int _fsmod_prep_obj_gfx_and_exec_cb(fsmod_file_chunk fChunk[1], gexdev_u32vec * iterVecp, uint32_t internalVars[INTERNAL_VAR_CNT], void * clientp){
    struct obj_gfx_scan_pack * packp = clientp;
    fsmod_file_chunk * mainChp = &packp->filesStp->mainChunk;
    void * headerData = NULL;
    void * bitmap = NULL;
    size_t bitmapSize = 0;
    struct gex_gfxHeader gfxHeader = {0};
    struct gfx_palette pal = {0};
    jmp_buf ** errbufpp = &packp->filesStp->error_jmp_buf;

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
    // BITMAP IN OTHER FILE CHUNK
    // TODO: DUMP IT TOO
    if((gfxHeader.typeSignature & 0x000000F0) == 0xC0) return 1;
    fseek(mainChp->dataFp, headerOffset, SEEK_SET);

    if(!gex_gfxHeadersFToAOB(mainChp->dataFp, &headerData)){
        return 1;
    }
    // bitmap read
    bitmapSize = gfx_checkSizeOfBitmap(headerData);
    if(fread(bitmap, bitmapSize, 1, mainChp->dataFp) < bitmapSize)
        longjmp(**errbufpp, FSMOD_READ_ERROR_FREAD);

    // palette parse
    fseek(mainChp->dataFp, paletteOffset, SEEK_SET);
    gfx_palette_parsef(mainChp->dataFp, &pal);

    // callback call
    packp->cb(packp->pass2cb, bitmap, headerData, &pal, iterVecp);

    //cleanup
    free(headerData);
    free(bitmap);

    return 1;
}

void fsmod_obj_gfx_scan(struct fsmod_files * filesStp, void *pass2cb,
                        void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                              const struct gfx_palette *palette, gexdev_u32vec * itervecp))
{
    struct obj_gfx_scan_pack scan_pack = {filesStp, pass2cb, cb};

    fsmod_follow_pattern_recur(&filesStp->mainChunk, "e+0x20gg [G{ g [G{ [G{ +20 [G{ [G{ +8c };]  };2]  };] };] }+4;]",
                               &scan_pack, _fsmod_prep_obj_gfx_and_exec_cb, &filesStp->error_jmp_buf);
}