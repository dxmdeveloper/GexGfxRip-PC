#include "gfx.h"
#include <png.h>
#include "../helpers/basicdefs.h"
#include "../helpers/binary_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


//* STATIC DECLARATIONS:
//
static void chunkRelDrawPixel(u8 **img, const struct gex_gfxChunk *chunk, u16 pix_i, u8 pixVal, u8 bpp);
void **calloc2D(u32 y, u32 x, u8 sizeOfElement);
void **malloc2D(u32 y, u32 x, u8 sizeOfElement);
png_color bgr555toRgb888(u16 bgr555);


//* EXTERN DEFINITIONS:
//
struct gfx_palette gfx_createPalette(void *gexPalette){
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
struct gex_gfxHeader *gex_gfxHeader_parsef(FILE * ifstream, struct gex_gfxHeader * dest){
    fread_LE_U16(&dest->_structPadding, 1, ifstream);
    fread_LE_U32(&dest->inf_imgWidth, 1, ifstream);
    fread_LE_U32(&dest->inf_imgHeight, 1, ifstream);
    fread_LE_I32(&dest->bitmap_shiftX, 1, ifstream);
    fread_LE_I16(&dest->bitmap_shiftY, 1, ifstream);
    if(!fread_LE_U32(&dest->typeSignature, 1, ifstream))  return NULL;
        
    return dest;
}
struct gex_gfxChunk *gex_gfxChunk_parsef(FILE * ifstream, struct gex_gfxChunk * dest){
    fread_LE_U16(&dest->startOffset, 1, ifstream);
    fread(&dest->width, sizeof(u8), 1, ifstream);
    fread(&dest->height, sizeof(u8), 1, ifstream);
    fread_LE_I16(&dest->rel_positionX, 1, ifstream);
    if(!fread_LE_I16(&dest->rel_positionY, 1, ifstream)) return NULL;

    return dest;
}

struct gex_gfxHeader gex_gfxHeader_parseAOB(const uint8_t aob[20]){
    struct gex_gfxHeader headerSt = {0};
    headerSt._structPadding = aob_read_LE_U16(aob);
    headerSt.inf_imgWidth   = aob_read_LE_U32(aob + 2);
    headerSt.inf_imgHeight  = aob_read_LE_U32(aob + 6);
    headerSt.bitmap_shiftX  = aob_read_LE_I32(aob + 10);
    headerSt.bitmap_shiftY  = aob_read_LE_I16(aob + 14);
    headerSt.typeSignature  = aob_read_LE_U32(aob + 16);
    return headerSt;
}
struct gex_gfxChunk gex_gfxChunk_parseAOB(const uint8_t aob[8]){
    struct gex_gfxChunk chunkSt = {0};
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

    *dest = gfx_createPalette(palData);
    free(palData);
    return dest;

}

size_t gfx_checkSizeOfBitmap(const void * gfxHeaders){
    u32 width = 0;
    u32 height = 0;
    struct gex_gfxHeader header = gex_gfxHeader_parseAOB(gfxHeaders);
    u8 bpp = gex_gfxHeaderType_getBpp(header.typeSignature);

    if(gfx_calcRealWidthAndHeight(&width, &height ,gfxHeaders + 20)){
        //error
        return 0;
    }
    if(bpp >= 8) return (size_t)(width * height * (bpp/8) );
    u32 modulo = width * height % (8/bpp);
    // we add 4 if header and bitmap are separate
    return (size_t)(width * height / (8/bpp) + modulo); 
}

// TODO: reimplementation of below functions (done?)

uint8_t **gfx_drawImgFromRaw(const void *gfxHeaders, const uint8_t bitmapDat[]){
    struct gex_gfxHeader header = {0};
    u32 realWidth = 0, realHeight = 0;
    u8 bpp = 0;

    if(gfxHeaders == NULL || bitmapDat == NULL) return NULL;

    header = gex_gfxHeader_parseAOB((u8*)gfxHeaders);
    gfxHeaders += 20;
    
    // Setting min sizes
    if(header.inf_imgWidth > IMG_MAX_WIDTH || header.inf_imgHeight > IMG_MAX_HEIGHT) return NULL;

    gfx_calcRealWidthAndHeight(&realWidth, &realHeight, gfxHeaders);
    header.inf_imgWidth = MAX(header.inf_imgWidth, realWidth);
    header.inf_imgHeight = MAX(header.inf_imgHeight, realHeight);

    switch (header.typeSignature & 7)
    {
        case 5:
            return gfx_drawSprite(gfxHeaders, bitmapDat, 8, header.inf_imgWidth, header.inf_imgHeight);
        case 4:
            return gfx_drawSprite(gfxHeaders, bitmapDat, 4, header.inf_imgWidth, header.inf_imgHeight);
        case 2:
            return (u8**)gfx_drawGexBitmap16bpp(gfxHeaders, bitmapDat, header.inf_imgWidth, header.inf_imgHeight);
        case 1:
            return gfx_drawGexBitmap(gfxHeaders, bitmapDat, 8, header.inf_imgWidth, header.inf_imgHeight);
        case 0:
            return gfx_drawGexBitmap(gfxHeaders, bitmapDat, 4, header.inf_imgWidth, header.inf_imgHeight);
    }
    return NULL;

}

size_t gex_gfxHeadersFToAOB(FILE * gfxHeadersFile, void ** dest){
    struct gex_gfxHeader header = {0};
    struct gex_gfxChunk chunk = {0};
    u8 * headersBuffor = NULL;
    size_t headersSize = 28;
    u16 opMapSize = 0;
    uptr filePositionSave = ftell(gfxHeadersFile);
    
    //gfxHeader parse
    if(!gex_gfxHeader_parsef(gfxHeadersFile, &header)) return 0;

    //gfxChunks parse
    if(!gex_gfxChunk_parsef(gfxHeadersFile, &chunk)) return 0;
    if(chunk.height == 0 || chunk.width == 0) return 0;
    while(chunk.height){
        if(!gex_gfxChunk_parsef(gfxHeadersFile, &chunk)) return 0;
        headersSize += 8;
    }
    if(header.typeSignature & 1){
        if(!fread(&opMapSize, 1, 1, gfxHeadersFile)) return 0;
        headersSize += 1 + opMapSize;
    }

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

uint8_t **gfx_drawImgFromRawf(FILE * gfxHeadersFile, const uint8_t * bitmapDat){
   void * gfxHeadersArr = NULL;
   size_t gfxHeadersSize = 0;
   u8** retVal = NULL;

   u8 *newBitmapDat = NULL;

   if(!(gfxHeadersSize = gex_gfxHeadersFToAOB(gfxHeadersFile, &gfxHeadersArr))) {
       return NULL;
   }
   if(bitmapDat == NULL){
       size_t bitmapSize = gfx_checkSizeOfBitmap(gfxHeadersArr);

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
   retVal = gfx_drawImgFromRaw(gfxHeadersFile, (bitmapDat ? bitmapDat : newBitmapDat));

   free(gfxHeadersArr);
   if(bitmapDat == NULL) free(newBitmapDat);

   return retVal;
}

u8 **gfx_drawGexBitmap(const void * chunkHeaders, const u8 bitmapIDat[], uint8_t bpp, u32 minWidth, u32 minHeight){
    u8 **image = NULL;
    u32 width = 0;
    u32 height = 0;
    u16 chunkIndex = 0;
    struct gex_gfxChunk chunk = {0};
    
    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawGexBitmap)\n");
        return NULL;
    }

    if(gfx_calcRealWidthAndHeight(&width, &height, chunkHeaders)){
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
    
    chunk = gex_gfxChunk_parseAOB(chunkHeaders);
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
            chunkRelDrawPixel(image, &chunk, i, dataPtr[i/(8/bpp)], bpp);
        }

        chunk = gex_gfxChunk_parseAOB(chunkHeaders + (++chunkIndex * 8));
    }

    return image;
} 

void **gfx_drawGexBitmap16bpp(const void * chunkHeaders, const void * bitmapDat, uint32_t minWidth, uint32_t minHeight){
    void **image = NULL;
    u32 width = 0, height = 0;
    struct gex_gfxChunk chunk = {0};

    if(!chunkHeaders || !bitmapDat) return NULL;
    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawSprite)\n");
        return NULL;
    }

    if(gfx_calcRealWidthAndHeight(&width, &height, chunkHeaders)){
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

// TODO: REWRITE
u8 **gfx_drawSprite(const void *chunksHeadersAndOpMap, const u8 bitmapIDat[], uint8_t bpp, u32 minWidth, u32 minHeight){
    u8 **image = NULL;
    const u8 *operationMapPtr = NULL;
    const u8 *bitmapBasePtr = NULL;
    u32 operationMapLen = 0;
    u32 bitmap_i = 0;
    u16 omap_i = 0;

    u32 width = 0;
    u32 height = 0;
    struct gex_gfxChunk chunk = {0};

    if(!chunksHeadersAndOpMap || !bitmapIDat) return NULL;
    bitmapBasePtr = bitmapIDat;

    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawSprite)\n");
        return NULL;
    }
    
    if(gfx_calcRealWidthAndHeight(&width, &height, chunksHeadersAndOpMap)){
        return NULL;
    }
    
    if(width < minWidth) width = minWidth;
    if(height < minHeight) height = minHeight;
    
    // malloc image with valid size
    image = (u8**)calloc2D(height, width, sizeof(u8));
    if(image==NULL){
        fprintf(stderr, "Out Of Memory!\n");
        return NULL;
    }
    
    // operation map length assignment
    {
        struct gex_gfxChunk firstChunk = {0};

        operationMapPtr = chunksHeadersAndOpMap;
        chunk = gex_gfxChunk_parseAOB(operationMapPtr);
        firstChunk = chunk;
        while(chunk.startOffset) {
            operationMapPtr += 8;
            chunk = gex_gfxChunk_parseAOB(operationMapPtr);
        };
        operationMapLen = *(u32*)(operationMapPtr+8);
        operationMapPtr += 9; 

        chunk = firstChunk;
    }

    u32 cpix_i = 0; //< chunk pixel iter

    // Proccess Data.
    while(omap_i < operationMapLen){

        // type of operations
        if(operationMapPtr[omap_i] < 0x80){
            // operation: simply draw pixels from bitmap
            u32 opCount = (operationMapPtr[omap_i] == 0 ? 128 * bpp  : operationMapPtr[omap_i] * 32 / bpp);

            for(u32 op = 0; op < opCount; op++){
                if(cpix_i >= chunk.height*chunk.width) { 
                    //next chunk
                    chunksHeadersAndOpMap+=8;
                    chunk = gex_gfxChunk_parseAOB(chunksHeadersAndOpMap);
                    if(chunk.startOffset == 0 || chunk.height*chunk.width == 0) 
                        return image;
                    cpix_i = 0;
                } 

                chunkRelDrawPixel(image, &chunk, cpix_i, bitmapBasePtr[bitmap_i/(8/bpp)], bpp);
                bitmap_i++;
                cpix_i++;
            }
        }
        // operation: repeat 4 pixels
        else {
            u32 repeats = (operationMapPtr[omap_i] == 0x80 ? bpp * 128 : (operationMapPtr[omap_i] - 0x80) * 32 / bpp);
            for(u32 op = 0; op < repeats; op++){ 
                if(cpix_i >= chunk.height*chunk.width) { 
                    //next chunk
                    chunksHeadersAndOpMap += 8;
                    chunk = gex_gfxChunk_parseAOB(chunksHeadersAndOpMap);
                    if(chunk.startOffset == 0 || chunk.height*chunk.width == 0) 
                        return image;
                    cpix_i = 0;
                } 

                chunkRelDrawPixel(image, &chunk, cpix_i, bitmapBasePtr[(bitmap_i+(op%32 / bpp))/(8/bpp)], bpp);
                cpix_i++;
            }
            bitmap_i+= 32 / bpp;
        }
        omap_i++;   
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

uint8_t gex_gfxHeaderType_getBpp(uint32_t typeSignature){
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
static void chunkRelDrawPixel(u8 **img, const struct gex_gfxChunk *chunk, u16 pix_i, u8 pixVal, u8 bpp){
    u16 y = chunk->rel_positionY + (pix_i / chunk->width);
    u16 x = chunk->rel_positionX + (pix_i % chunk->width);
    
    if(bpp == 8) img[y][x] = pixVal;
    else {
        u8 shift = bpp * (pix_i % (8/bpp));
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

bool gfx_calcRealWidthAndHeight(u32 *ref_width, u32 *ref_height, const void *firstChunk){
    struct gex_gfxChunk chunk = {0};
    u8 chunk_i = 0;
    
    chunk = gex_gfxChunk_parseAOB((u8*)firstChunk);
    while(chunk.startOffset > 0){
        if(chunk_i > IMG_CHUNKS_LIMIT){
            fprintf(stderr, "Error: Chunks limit reached (gfx.c::checkSizeOfCanvas)\n"); 
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
        chunk = gex_gfxChunk_parseAOB((u8*)firstChunk);
        chunk_i++;
    }
    // canvas size validatation
    if(*ref_width < 1 || *ref_height < 1 || *ref_width >  IMG_MAX_WIDTH || *ref_height > IMG_MAX_HEIGHT){
        // invalid graphic format / misrecognized data
        return true;
    }
    return false;
}