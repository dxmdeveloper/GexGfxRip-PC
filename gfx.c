#include "gfx.h"
#include <png.h>
#include "basicdefs.h"
#include "binary_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


//* PRIVATE DECLARATIONS:
//
void chunkRelDrawPixel(u8 **img, const struct gex_gfxChunk *chunk, u16 pix_i, u8 pixVal, bool is4bpp);
bool checkSizeOfCanvas(u32 *ref_width, u32 *ref_height, const struct gex_gfxChunk *firstChunk);
void **calloc2D(u32 y, u32 x, u8 sizeOfElement);
void **malloc2D(u32 y, u32 x, u8 sizeOfElement);
png_color bgr555toRgb888(u16 bgr555);



//* PUBLIC DEFINITIONS:
//
struct gfx_palette gfx_createPalette(void *gexPalette){
    struct gfx_palette newPalette;
    u16 *colors = (u16*)(gexPalette+4);
    newPalette.tRNS_count = 0;
    

    //exception: null pointer
    if(gexPalette == NULL) {
        newPalette.colorsCount = 0;
        return newPalette;
    }

    switch(*((u32*)gexPalette + 0) | 0xff00){
        case 0xffffff00:
            newPalette.colorsCount = 16;
            break;
        case 0xffffff01:
            newPalette.colorsCount = 256;
            break;
        default:
            newPalette.colorsCount = 0;
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
    if(!fread_LE_U16(&dest->_structPadding, 1, ifstream)) return NULL;
    if(!fread_LE_U32(&dest->inf_imgWidth, 1, ifstream))   return NULL;
    if(!fread_LE_U32(&dest->inf_imgHeight, 1, ifstream))  return NULL;
    if(!fread_LE_I32(&dest->bitmap_shiftX, 1, ifstream))  return NULL;
    if(!fread_LE_I16(&dest->bitmap_shiftY, 1, ifstream))  return NULL;
    if(!fread_LE_U32(&dest->typeSignature, 1, ifstream))  return NULL;
        
    return dest;
}
struct gex_gfxChunk *gex_gfxChunk_parsef(FILE * ifstream, struct gex_gfxChunk * dest){
    if(!fread_LE_U16(&dest->startOffset, 1, ifstream))   return NULL;
    if(!fread(&dest->width, sizeof(u8), 1, ifstream))    return NULL;
    if(!fread(&dest->height, sizeof(u8), 1, ifstream))   return NULL;
    if(!fread_LE_I16(&dest->rel_positionX, 1, ifstream)) return NULL;
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

size_t gfx_checkSizeOfBitmap(const void * gfxHeaders){
    u32 width = 0;
    u32 height = 0;
    //struct gex_gfxHeader header = gex_gfxHeader_parseAOB(gfxHeaders);
    if(checkSizeOfCanvas(&width, &height ,(struct gex_gfxChunk*)gfxHeaders + 20)){
        //error
        return 0;
    }
    return (size_t) width * height;
}

// TODO: reimplementation of below functions

uint8_t **gfx_drawImgFromRaw(const void *gfxHeaders, const uint8_t bitmapDat[]){
    struct gex_gfxHeader header = {0};

    if(gfxHeaders == NULL || bitmapDat == NULL) return NULL;

    header = gex_gfxHeader_parseAOB((u8*)gfxHeaders);
    gfxHeaders += 20;
    
    if(header.inf_imgWidth < 2 || header.inf_imgHeight < 2 
    || header.inf_imgWidth > IMG_MAX_WIDTH || header.inf_imgHeight > IMG_MAX_HEIGHT) return NULL;

    switch (header.typeSignature & 0x00000005)
    {
        case 5:
            return gfx_drawSprite(gfxHeaders, bitmapDat, false, header.inf_imgWidth, header.inf_imgHeight);
        case 4:
            return gfx_drawSprite(gfxHeaders, bitmapDat, true, header.inf_imgWidth, header.inf_imgHeight);
        case 1:
            return gfx_drawGexBitmap(gfxHeaders, bitmapDat, false, header.inf_imgWidth, header.inf_imgHeight);
        case 0:
            return gfx_drawGexBitmap(gfxHeaders, bitmapDat, true, header.inf_imgWidth, header.inf_imgHeight);
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
    if(header.typeSignature | 1){
        if(!fread(&opMapSize, 1, 1, gfxHeadersFile)) return 0;
        headersSize += 1 + opMapSize;
    }

    // file position restore and read all into the headersBuffor
    fseek(gfxHeadersFile, filePositionSave, SEEK_SET);
    headersBuffor = malloc(headersSize);
    if(fread(headersBuffor, 1, headersSize, gfxHeadersFile)){
        free(headersBuffor);
        return 0;
    }

    // successful end of function
    dest = (void**) headersBuffor;
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

u8 **gfx_drawGexBitmap(const void * chunkHeaders, const u8 bitmapIDat[], bool is4bpp, u32 minWidth, u32 minHeight){
    u8 **image = NULL;
    u32 width = 0;
    u32 height = 0;
    u16 chunkIndex = 0;
    struct gex_gfxChunk chunk = {0};
    
    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawGexBitmap)\n");
        return NULL;
    }

    if(checkSizeOfCanvas(&width, &height, chunkHeaders)){
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

    //foreach chunk
    while(chunk.startOffset > 0){
        const u8 *dataPtr = bitmapIDat;

        
        if(chunk.startOffset > (width*height) / (is4bpp ? 2 : 1)){
            //Invalid graphic / misrecognized data
            free(image);
            return NULL;
        } 

        // Proccess Data
        for(u16 i = 0; i < chunk.height*chunk.width; i++){
            u16 y = chunk.rel_positionY + (i / chunk.width);
            u16 x = chunk.rel_positionX + (i % chunk.width);
            
            image[y][x] = dataPtr[i];
            chunkRelDrawPixel(image, &chunk, i, dataPtr[i/(is4bpp ? 2 : 1)], is4bpp);
        }

        chunk = gex_gfxChunk_parseAOB(chunkHeaders + (++chunkIndex * 8));
    }

    return image;
} /**/


u8 **gfx_drawSprite(const void *chunksHeadersAndOpMap, const u8 bitmapIDat[], bool is4bpp, u32 minWidth, u32 minHeight){
    u8 **image = NULL;
    const u8 *operationMapPtr = NULL;
    const u8 *bitmapBasePtr = NULL;
    u32 operationMapLen = 0;
    u32 bitmap_i = 0;
    u16 omap_i = 0;

    u32 width = 0;
    u32 height = 0;
    struct gex_gfxChunk chunk = {0};

    if(minWidth > IMG_MAX_WIDTH || minHeight > IMG_MAX_HEIGHT){
        fprintf(stderr, "Err: minWidth/minHeight argument exceeds IMG_MAX_ limit (gfx.c::gfx_drawSprite)\n");
        return NULL;
    }
    
    
    if(checkSizeOfCanvas(&width, &height, chunksHeadersAndOpMap)){
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
            u32 opCount = (operationMapPtr[omap_i] == 0 ? (is4bpp ? 1024 : 512)  : operationMapPtr[omap_i] * (is4bpp ? 8 : 4));

            for(u32 op = 0; op < opCount; op++){
                if(cpix_i >= chunk.height*chunk.width) { 
                    //next chunk
                    chunksHeadersAndOpMap+=8;
                    chunk = gex_gfxChunk_parseAOB(chunksHeadersAndOpMap);
                    if(chunk.startOffset == 0 || chunk.height*chunk.width == 0) 
                        return image;
                    cpix_i = 0;
                } 

                chunkRelDrawPixel(image, &chunk, cpix_i, bitmapBasePtr[bitmap_i/(!is4bpp?1:2)], is4bpp);
                bitmap_i++;
                cpix_i++;
            }
        }
        // operation: repeat 4 pixels
        else {
            u32 repeats = (operationMapPtr[omap_i] == 0x80 ? (is4bpp ? 1024 : 512) : (operationMapPtr[omap_i] - 0x80) * (is4bpp ? 8 : 4));
            for(u32 op = 0; op < repeats; op++){ 
                if(cpix_i >= chunk.height*chunk.width) { 
                    //next chunk
                    chunksHeadersAndOpMap += 8;
                    chunk = gex_gfxChunk_parseAOB(chunksHeadersAndOpMap);
                    if(chunk.startOffset == 0 || chunk.height*chunk.width == 0) 
                        return image;
                    cpix_i = 0;
                } 

                chunkRelDrawPixel(image, &chunk, cpix_i, bitmapBasePtr[(bitmap_i+(op%(is4bpp ? 8 : 4)))/(!is4bpp?1:2)], is4bpp);
                cpix_i++;
            }
            bitmap_i+=(is4bpp ? 8 : 4);
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



// -----------------------------------------------------
// STATIC FUNCTION DEFINITIONS:
// -----------------------------------------------------

// draw pixel relative of chunk position
void chunkRelDrawPixel(u8 **img, const struct gex_gfxChunk *chunk, u16 pix_i, u8 pixVal, bool is4bpp){
    u16 y = chunk->rel_positionY + (pix_i / chunk->width);
    u16 x = chunk->rel_positionX + (pix_i % chunk->width);
    
    if(!is4bpp)
        img[y][x] = pixVal;
    else{
        img[y][x] = (pix_i % 2 ? pixVal >> 4 : pixVal & 0x0f); 
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

//returns true on error
bool checkSizeOfCanvas(u32 *ref_width, u32 *ref_height, const struct gex_gfxChunk *firstChunk){
    struct gex_gfxChunk chunk = {0};
    u8 chunk_i = 0;
    
    while(chunk.startOffset > 0){
        chunk = gex_gfxChunk_parseAOB((u8*)firstChunk);
        if(chunk_i > IMG_CHUNKS_LIMIT){
            fprintf(stderr, "Error: Chunks limit reached (gfx.c::checkSizeOfCanvas)\n"); 
            return true;
        }
        // Compare min required size with current canvas borders
        if(chunk.rel_positionX + chunk.width > *ref_width){
            *ref_width = chunk.rel_positionX + chunk.width;
        }
        if(chunk.rel_positionY + chunk.height > *ref_height){
            *ref_height = chunk.rel_positionY + chunk.height + 1;
        }
        // chunk validatation
        if(chunk.startOffset < 20){
            // invalid graphic format / misrecognized data
            return true;
        }        
        firstChunk++;
        chunk_i++;
    }
    // canvas size validatation
    if(*ref_width < 1 || *ref_height < 1 || *ref_width >  IMG_MAX_WIDTH || *ref_height > IMG_MAX_HEIGHT){
        // invalid graphic format / misrecognized data
        return true;
    }
    return false;
}