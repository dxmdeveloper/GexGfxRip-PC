#include <stdio.h>
#include <png.h>
#include <stdlib.h>
#include "basicdefs.h"
#include "filescanning.h"
#include "write_png.h"
#include "gfx.h"


// PRIVATE DECLARATIONS:
struct gfx_palette createDefaultPalette(bool _256colors, bool transparency);
void onfound(void *gfx, const struct gfx_palette *palette, const char ofilname[]);
void printUsageHelp(){
    printf("USAGE: ."PATH_SEP"gexgfxrip [path to file]\n");
}


// CONSTANTS (only one assigment)
struct gfx_palette const_grayscalePal256;
struct gfx_palette const_grayscalePal16;

//-------------------- Program Entry Point --------------------------
int main(int argc, char *argv[]) {
    //TODO: check arg string length
    if(argc < 2) {
        printUsageHelp();
        return -1;
    }

    const_grayscalePal256 = createDefaultPalette(true, true);
    const_grayscalePal16 = createDefaultPalette(false, true);

    scan4Gfx(argv[argc-1], onfound);
    return 0;     
}
//-------------------------------------------------------------------





// callback function for scan4Gfx
// TODO: filename arg
void onfound(void *gfx, const struct gfx_palette *palette, const char ofilname[]){
    static u32 counter = 0;
    struct gex_gfxHeader *gfxHeader = gfx;
    struct gfx_palette *loc_palp = palette; //< local pointer var for color palette

    void **image = NULL;

    /*
    // ! TESTING
    gfxHeader->inf_imgWidth = 512;
    gfxHeader->inf_imgHeight = 512;
    */

    // exceptions handling 
    if(palette == NULL) {
        fprintf(stderr, "Err: invalid gex color palette\n");
        if(gfxHeader->typeSignature & 1) loc_palp = &const_grayscalePal256;
        else loc_palp = &const_grayscalePal16;
    }
    else if((gfxHeader->typeSignature & 1) && loc_palp->colorsCount < 256){
        fprintf(stderr, "Err: color palette and graphic types mismatch\n");
        return;
    }
    
    // Image creation
    image = gfx_drawImgFromRaw(gfx);
    if(image == NULL) return;

    
    // PNG creation
    WritePng(ofilname, image, 
     gfxHeader->inf_imgWidth, gfxHeader->inf_imgHeight,
     loc_palp->palette, loc_palp->colorsCount, loc_palp->tRNS_array, loc_palp->tRNS_count);

    //cleaning
    free(image);
    
    counter++;
}

struct gfx_palette createDefaultPalette(bool _256colors, bool transparency){
    struct gfx_palette pal = {0};
    if(transparency){
        pal.tRNS_count = 1;
        pal.tRNS_array[0] = 0;
    }
    pal.colorsCount = (_256colors ? 256 : 16);
    for(u16 i = 0; i < 256; i+= (_256colors ? 1 : 16)){
        pal.palette[i].blue = i;
        pal.palette[i].green = i;
        pal.palette[i].red = i;
    }
    return pal;
}
