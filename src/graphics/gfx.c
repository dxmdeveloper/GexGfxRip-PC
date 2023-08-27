#include "gfx.h"
#include <png.h>
#include "../helpers/basicdefs.h"
#include "../helpers/binary_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


//* STATIC DECLARATIONS:
//
static void chunk_rel_draw_pixel(u8 **img, const struct gex_gfxchunk *chunk, u16 pixInChunkI, u8 pixVal, u8 bpp);
void **calloc2D(u32 y, u32 x, u8 sizeOfElement);
void **malloc2D(u32 y, u32 x, u8 sizeOfElement);
png_color bgr555toRgb888(u16 bgr555);


//* EXTERN DEFINITIONS:
//
struct gfx_palette gfx_create_palette(void *gexPalette){
    struct gfx_palette newPalette = {0};
    u16 *colors = (u16*)(gexPalette+4);
    newPalette.tRNS_count = 0;
    
    //exception: null pointer
    if(gexPalette == NULL) {
        newPalette.colorsCount = 0;
        return newPalette;
    }

    switch(aob_read_LE_U32(gexPalette) | 0xff00){
        case 0xffffff00:
            newPalette.colorsCount = 16;
            break;
        case 0xffffff01:
            newPalette.colorsCount = 256;
            break;
        default: //0xffffff02 is an placeholder for 16 bpp bitmap
            return newPalette;
    }
    
    //palette creation
    for(u16 i = 0; i < newPalette.colorsCount; i++){
        newPalette.palette[i] = bgr555toRgb888(colors[i]);
        //transparency
        if(colors[i] == 0){
            newPalette.tRNS_count = i + 1;
            newPalette.tRNS_array[i] = 0;
        }
        else {
            newPalette.tRNS_array[i] = 0xFF;
        }
    }

    return newPalette;
}

/*---- structures deserialization ----*/
struct gex_gfxheader *gex_gfxheader_parsef(FILE * ifstream, struct gex_gfxheader * dest){
    fread_LE_U16(&dest->_structPadding, 1, ifstream);
    fread_LE_U32(&dest->inf_imgWidth, 1, ifstream);
    fread_LE_U32(&dest->inf_imgHeight, 1, ifstream);
    fread_LE_I32(&dest->bitmap_shiftX, 1, ifstream);
    fread_LE_I16(&dest->bitmap_shiftY, 1, ifstream);
    if(!fread_LE_U32(&dest->typeSignature, 1, ifstream))  return NULL;
        
    return dest;
}
struct gex_gfxchunk *gex_gfxchunk_parsef(FILE * ifstream, struct gex_gfxchunk * dest){
    fread_LE_U16(&dest->startOffset, 1, ifstream);
    fread(&dest->width, sizeof(u8), 1, ifstream);
    fread(&dest->height, sizeof(u8), 1, ifstream);
    fread_LE_I16(&dest->rel_positionX, 1, ifstream);
    if(!fread_LE_I16(&dest->rel_positionY, 1, ifstream)) return NULL;

    return dest;
}

struct gex_gfxheader gex_gfxheader_parse_aob(const uint8_t aob[20]){
    struct gex_gfxheader headerSt = {0};
    headerSt._structPadding = aob_read_LE_U16(aob);
    headerSt.inf_imgWidth   = aob_read_LE_U32(aob + 2);
    headerSt.inf_imgHeight  = aob_read_LE_U32(aob + 6);
    headerSt.bitmap_shiftX  = aob_read_LE_I32(aob + 10);
    headerSt.bitmap_shiftY  = aob_read_LE_I16(aob + 14);
    headerSt.typeSignature  = aob_read_LE_U32(aob + 16);
    return headerSt;
}
struct gex_gfxchunk gex_gfxchunk_parse_aob(const uint8_t aob[8]){
    struct gex_gfxchunk chunkSt = {0};
    chunkSt.startOffset = aob_read_LE_U16(aob);
    chunkSt.width  = aob[2];
    chunkSt.height = aob[3];
    chunkSt.rel_positionX = aob_read_LE_I16(aob + 4);
    chunkSt.rel_positionY = aob_read_LE_I16(aob + 6);

    return chunkSt;
}

struct gfx_palette * gfx_palette_parsef(FILE * ifstream, struct gfx_palette * dest){
    void * palData = NULL;
    size_t pal_size = 0;
    u32 type = 0;

    if(!fread_LE_U32(&type, 1, ifstream)) return NULL;
    pal_size = 4 + (type & 1 ? 256 : 16) * 2;
    palData = malloc(pal_size);
    fseek(ifstream, -4, SEEK_CUR);
    if(fread(palData, 1, pal_size, ifstream) < pal_size){
        free(palData); return NULL;
    }

    *dest = gfx_create_palette(palData);
    free(palData);
    return dest;

}

size_t gfx_calc_size_of_bitmap(const void * gfxHeaders){
    size_t size = 0;
    struct gex_gfxheader header = gex_gfxheader_parse_aob(gfxHeaders);
    struct gex_gfxchunk gchunk = gex_gfxchunk_parse_aob(gfxHeaders+20);
    u8 bpp = gex_gfxheader_type_get_bpp(header.typeSignature);

    for(uint i = 1; gchunk.startOffset; i++){
        size += gchunk.width*gchunk.height;
        gchunk = gex_gfxchunk_parse_aob(gfxHeaders+20+8*i);
    }

    if(bpp >= 8) return size * (bpp/8);
    u32 modulo = size % (8/bpp);
    return size / (8/bpp) + modulo;
}

size_t gfx_calc_size_of_sprite(const void * gfxHeadersAndOpMap){
    struct gex_gfxheader header = gex_gfxheader_parse_aob(gfxHeadersAndOpMap);
    struct gex_gfxchunk gchunk = gex_gfxchunk_parse_aob(gfxHeadersAndOpMap+20);
    u8 bpp = gex_gfxheader_type_get_bpp(header.typeSignature);
    const u8 * opmap = NULL;
    size_t chunkCnt = 0;
    size_t bytes = 0;
    u32 opmapSize = 0;

    while(gchunk.startOffset){
        chunkCnt++;
        gchunk = gex_gfxchunk_parse_aob(gfxHeadersAndOpMap+20+chunkCnt*8);   
    }

    opmap = gfxHeadersAndOpMap+20+(chunkCnt+1)*8+4;
    opmapSize = aob_read_LE_U32(opmap-4);
    if(opmapSize == 0) return 0;
    
    for(u32 i = 0; i < opmapSize - 4; i++){
        if(opmap[i] < 0x80) bytes += (opmap[i] == 0 ? 4096 / bpp  : opmap[i] * 32 / bpp);
        else bytes += bpp / 2;
    }

    return bytes;
}

uint8_t **gfx_draw_img_from_raw(const void *gfxHeaders, const uint8_t bitmapDat[]){
    struct gex_gfxheader header = {0};
    u32 realWidth = 0, realHeight = 0;
    u8 bpp = 0;

    if(gfxHeaders == NULL || bitmapDat == NULL) return NULL;

    header = gex_gfxheader_parse_aob((u8*)gfxHeaders);
    gfxHeaders += 20;

    //if(header.inf_imgWidth > IMG_MAX_WIDTH || header.inf_imgHeight > IMG_MAX_HEIGHT) return NULL;

    gfx_calc_real_width_and_height(&realWidth, &realHeight, gfxHeaders);

    switch (header.typeSignature & 7)
    {
        case 5:
            return gfx_draw_sprite(gfxHeaders, bitmapDat, 8, realWidth, realHeight);
        case 4:
            return gfx_draw_sprite(gfxHeaders, bitmapDat, 4, realWidth, realHeight);
        case 2:
            return (u8**)gfx_draw_gex_bitmap_16bpp(gfxHeaders, bitmapDat, realWidth, realHeight);
        case 1:
            return gfx_draw_gex_bitmap(gfxHeaders, bitmapDat, 8, realWidth, realHeight);
        case 0:
            return gfx_draw_gex_bitmap(gfxHeaders, bitmapDat, 4, realWidth, realHeight);
    }
    return NULL;

}

size_t gfx_read_headers_alloc_aob(FILE * gfxHeadersFile, void ** dest){
    struct gex_gfxheader header = {0};
    struct gex_gfxchunk chunk = {0};
    u8 * headersBuffor = NULL;
    size_t headersSize = 28;
    u32 opMapSize = 0;
    uptr filePositionSave = ftell(gfxHeadersFile);
    

    //gfxHeader parse
    if(!gex_gfxheader_parsef(gfxHeadersFile, &header)) return 0;
    if(header._structPadding) return 0;

    //gfxChunks parse
    if(!gex_gfxchunk_parsef(gfxHeadersFile, &chunk)) return 0;
    if(chunk.height == 0 || chunk.width == 0) return 0;
    while(chunk.height){
        if(!gex_gfxchunk_parsef(gfxHeadersFile, &chunk)) return 0;
        headersSize += 8;
    }

    if(headersSize > 28 + IMG_CHUNKS_LIMIT * 8) return 0;

    if(header.typeSignature & 4){
        if(!fread_LE_U32(&opMapSize, 1, gfxHeadersFile)) return 0;
        headersSize += opMapSize;
    }
    
    if(opMapSize > 0xFFFF /*?*/) return 0;

    // file position restore and read all into the headersBuffor
    fseek(gfxHeadersFile, filePositionSave, SEEK_SET);
    headersBuffor = malloc(headersSize);
    if(!fread(headersBuffor, 1, headersSize, gfxHeadersFile)){
        free(headersBuffor);
        return 0;
    }

    // successful end of function
    *dest = headersBuffor;
    return headersSize;
}

uint8_t **gfx_draw_img_from_rawf(FILE * gfxHeadersFile, const uint8_t * bitmapDat){
   void * gfxHeadersArr = NULL;
   size_t gfxHeadersSize = 0;
   u8** retVal = NULL;

   u8 *newBitmapDat = NULL;

   if(!(gfxHeadersSize = gfx_read_headers_alloc_aob(gfxHeadersFile, &gfxHeadersArr))) {
       return NULL;
   }
   if(bitmapDat == NULL){
       size_t bitmapSize = gfx_calc_size_of_bitmap(gfxHeadersArr);

       if(!bitmapSize){free(gfxHeadersArr); return NULL;} 

       newBitmapDat = malloc(bitmapSize);
       if(newBitmapDat == NULL){
           fprintf(stderr, "Err: failed to allocate memory (gfx.c::gfx_drawImgFromRawf)\n");
           exit(0xBEEF);
       }
       if(fread(newBitmapDat, 1, 1, gfxHeadersFile) != bitmapSize){
           free(gfxHeadersArr);
           free(newBitmapDat);
           return NULL;
       }
   }
   retVal = gfx_draw_img_from_raw(gfxHeadersFile, (bitmapDat ? bitmapDat : newBitmapDat));

   free(gfxHeadersArr);
   if(bitmapDat == NULL) free(newBitmapDat);

   return retVal;
}

u8 **gfx_draw_gex_bitmap(const void * chunkHeaders, const u8 bitmapIDat[], uint8_t bpp, u32 minWidth, u32 minHeight){
    u8 **image = NULL;
    u32 width = 0;
    u32 height = 0;
    u16 chunkIndex = 0;
    struct gex_gfxchunk chunk = {0};
    
    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawGexBitmap)\n");
        return NULL;
    }

    if(gfx_calc_real_width_and_height(&width, &height, chunkHeaders)){
        return NULL;
    }
    
    if(width < minWidth) width = minWidth;
    if(height < minHeight) height = minHeight;

    // malloc image with valid size
    image = (u8**) calloc2D(height, width, sizeof(u8));
    if(image==NULL){
        fprintf(stderr, "Out Of Memory!\n");
        return NULL;
    }
    
    chunk = gex_gfxchunk_parse_aob(chunkHeaders);
    uint bitmap_offset = chunk.startOffset; // This works different than the game engine

    //foreach chunk
    while(chunk.startOffset > 0){
        const u8 *dataPtr = bitmapIDat + chunk.startOffset - bitmap_offset;
        
        if(chunk.startOffset - bitmap_offset > (width*height) / (8/bpp)){
            //Invalid graphic / misrecognized data
            free(image);
            return NULL;
        } 

        // Proccess Data
        for(u16 i = 0; i < chunk.height*chunk.width; i++){
            u16 y = chunk.rel_positionY + (i / chunk.width);
            u16 x = chunk.rel_positionX + (i % chunk.width);
            
            image[y][x] = dataPtr[i];
            chunk_rel_draw_pixel(image, &chunk, i, dataPtr[i/(8/bpp)], bpp);
        }

        chunk = gex_gfxchunk_parse_aob(chunkHeaders + (++chunkIndex * 8));
    }

    return image;
} 

void **gfx_draw_gex_bitmap_16bpp(const void * chunkHeaders, const void * bitmapDat, uint32_t minWidth, uint32_t minHeight){
    void **image = NULL;
    u32 width = 0, height = 0;
    struct gex_gfxchunk chunk = {0};

    if(!chunkHeaders || !bitmapDat) return NULL;
    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawSprite)\n");
        return NULL;
    }

    if(gfx_calc_real_width_and_height(&width, &height, chunkHeaders)){
        return NULL;
    }

    image = calloc2D(MAX(height, minHeight), MAX(width, minWidth), 4);
    if(!image) return NULL;

    for(u32 y = 0; y < height; y++){
        for(u32 x = 0; x < width; x++){
            u16 rgb555val = aob_read_LE_U16(bitmapDat + (y * width + x) * 2);
            png_color color = bgr555toRgb888(rgb555val);
            ((u8**)image)[y][x*4 + 0] = color.red;
            ((u8**)image)[y][x*4 + 1] = color.green;
            ((u8**)image)[y][x*4 + 2] = color.blue;
            ((u8**)image)[y][x*4 + 3] = rgb555val ? 0xFF : 0;
        }
    }
    return image;
}

// TODO CONSIDER GFX HEADER ARG INSTEAD OF chunksAndOpMap AND bpp
u8 **gfx_draw_sprite(const void *chunksAndOpmap, const u8 * bmp, u8 bpp, u32 minWidth, u32 minHeight){
    u8 **image = NULL;
    u32 width = 0, height = 0;
    const u8 * opmapp = NULL;
    u32 opmapSize = 0;
    uint pixI = 0, pixInChunkI = 0;
    uint chunkIndex = 0;
    struct gex_gfxchunk chunk = {0};
    
    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawGexBitmap)\n");
        return NULL;
    }

    if(gfx_calc_real_width_and_height(&width, &height, chunksAndOpmap)){
        return NULL;
    }
    
    if(width < minWidth) width = minWidth;
    if(height < minHeight) height = minHeight;

    // malloc image with valid size
    image = (u8**) calloc2D(height, width, sizeof(u8));
    if(image==NULL){
        fprintf(stderr, "Out Of Memory!\n");
        return NULL;
    }
    
    // opmap size and pointer
    {
        struct gex_gfxchunk firstChunk = gex_gfxchunk_parse_aob(chunksAndOpmap);
        chunk = firstChunk;
        while(chunk.startOffset){
            chunkIndex++;
            chunk = gex_gfxchunk_parse_aob(chunksAndOpmap+chunkIndex*8);
        }
        opmapp = chunksAndOpmap+chunkIndex*8 + 12;
        opmapSize = aob_read_LE_U32(opmapp-4) - 4;

        chunk = firstChunk;
        chunkIndex = 0;
    }

    uint bitmap_offset = chunk.startOffset; // This works different than the game engine

    for(size_t m = 0; m < opmapSize; m++){
        uint optype = 0; // 0 - just draw pixels from bitmap, 1 - take 4 bytes and draw them over and over
        uint repeats = 0;
        u8 opval = opmapp[m];
        u8 pixVal = 0;

        if(opmapp[m] >= 0x80) {
            optype = 1;
            opval -= 0x80;
        }
        repeats = (opval ? opval * 32 : 4096) / bpp;
        for(uint i = 0; i < repeats; i++){
            // next graphic chunk
            if(pixInChunkI >= chunk.height*chunk.width){
                chunk = gex_gfxchunk_parse_aob(chunksAndOpmap+(++chunkIndex)*8);
                if(chunk.height*chunk.width == 0) return image;
                pixInChunkI = 0;
            }
            switch(optype){
                case 0: pixVal = bmp[pixI++ / (8/bpp)]; break;
                case 1: pixVal = bmp[(pixI + (i%32 / bpp))/(8/bpp)]; break;
            }
            chunk_rel_draw_pixel(image, &chunk, pixInChunkI, pixVal , bpp);
            pixInChunkI++;
        }
        if(optype == 1) pixI += 32 / bpp;
    }
    return image;
}


png_color bgr555toRgb888(u16 bgr555) {
    //1BBBBBGGGGGRRRRR
    png_color rgb888;
    rgb888.red   =  8 * ((bgr555) & 0b11111);
    rgb888.green =  8 * ((bgr555 >> 5) & 0b11111);
    rgb888.blue  =  8 * ((bgr555 >> 10) & 0b11111);

    return rgb888;
}

uint8_t gex_gfxheader_type_get_bpp(uint32_t typeSignature){
    switch(typeSignature & 3){
        case 3: //?
        case 2: return 16;
        case 1: return 8;
        case 0: return 4;
    }
    return 0;
}

// -----------------------------------------------------
// STATIC FUNCTION DEFINITIONS:
// -----------------------------------------------------

// draw pixel relative of chunk position
static void chunk_rel_draw_pixel(u8 **img, const struct gex_gfxchunk *chunk, u16 cpix_i, u8 pixVal, u8 bpp){
    u16 y = chunk->rel_positionY + (cpix_i / chunk->width);
    u16 x = chunk->rel_positionX + (cpix_i % chunk->width);
    
    if(bpp == 8) img[y][x] = pixVal;
    else {
        u8 shift = bpp * (cpix_i % (8/bpp));
        u8 mask = 0xFF >> (8 - bpp);
        img[y][x] = (pixVal >> shift) & mask;
    }
}

void **malloc2D(u32 y, u32 x, u8 sizeOfElement){
    void **arr = (void**)malloc(sizeof(uintptr_t)*y + sizeOfElement*x*y);
    uintptr_t addr = (uintptr_t) &arr[y];

    for(u32 i = 0; i < y; i++)
        arr[i] = (void *)(addr + i * sizeOfElement * x);

    return arr;
}
void **calloc2D(u32 y, u32 x, u8 sizeOfElement){
    void **arr = (void**)calloc(sizeof(uintptr_t)*y + sizeOfElement*x*y , 1);
    uintptr_t addr = (uintptr_t) &arr[y];

    for(u32 i = 0; i < y; i++)
        arr[i] = (void *)(addr + i * sizeOfElement * x);

    return arr;
}

bool gfx_calc_real_width_and_height(u32 *ref_width, u32 *ref_height, const void *firstChunk){
    struct gex_gfxchunk chunk = {0};
    u8 chunk_i = 0;
    
    chunk = gex_gfxchunk_parse_aob((u8*)firstChunk);
    while(chunk.startOffset > 0){
        if(chunk_i > IMG_CHUNKS_LIMIT){
            fprintf(stderr, "Error: Chunks limit reached (gfx.c::gfx_calc_real_width_and_height)\n"); 
            return true;
        }
        // Compare min required size with current canvas borders
        if(chunk.rel_positionX + chunk.width > *ref_width){
            *ref_width = chunk.rel_positionX + chunk.width;
        }
        if(chunk.rel_positionY + chunk.height > *ref_height){
            *ref_height = chunk.rel_positionY + chunk.height;
        }
        // chunk validatation
        if(chunk.startOffset < 20){
            // invalid graphic format / misrecognized data
            return true;
        }        
        firstChunk+=8;
        chunk = gex_gfxchunk_parse_aob((u8*)firstChunk);
        chunk_i++;
    }
    // canvas size validatation
    if(*ref_width < 1 || *ref_height < 1 || *ref_width >  IMG_MAX_WIDTH || *ref_height > IMG_MAX_HEIGHT){
        // invalid graphic format / misrecognized data
        return true;
    }
    return false;
}