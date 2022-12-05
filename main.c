#include <stdio.h>
#include <png.h>
#include <stdlib.h>
#include "basicdefs.h"
#include "filescanning.h"
#include "write_png.h"
#include "gfx.h"


// PRIVATE DECLARATIONS:
struct gfx_palette createDefaultPalette(bool _256colors, bool transparency);
void onfound(void *gfx, const struct gfx_palette *palette, const char ofilename[]);
void printUsageHelp(){
    printf("USAGE: ."PATH_SEP"gexgfxrip [path to file]\n");
}


// CONSTANTS (only one assigment)
struct gfx_palette const_grayscalePal256;
struct gfx_palette const_grayscalePal16;

//-------------------- Program Entry Point --------------------------
int main(int argc, char *argv[]) {
    
    const_grayscalePal256 = createDefaultPalette(true, true);
    const_grayscalePal16 = createDefaultPalette(false, true);

    // if no additional program arguments or
    if(argc == 1 || strcmp(argv[argc-1], "*") == 0){
        char ifilename[11];
        for(u8 fileI = 0; fileI < 255; fileI++){
            sprintf(ifilename, "GEX%03u.LEV", fileI);

            // Test file availibity
            FILE* testFile = fopen(ifilename, "rb");
            if(testFile == NULL) continue;
            fclose(testFile);

            // Scan found file
            scan4Gfx(ifilename, onfound);
        }
    } else {
        scan4Gfx(argv[argc-1], onfound);
    }
    return 0;     
}
//-------------------------------------------------------------------





// callback function for scan4Gfx
// TODO: output filename based on program argument
void onfound(void *gfx, const struct gfx_palette *palette, const char ofilename[]){
    struct gex_gfxHeader *gfxHeader = gfx;
    const struct gfx_palette *loc_palp = palette; //< local pointer var for color palette

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
    if(image == NULL) {
        dbg_errlog("DEBUG: failed to create %s", ofilename);
        return;
    }
    
    // PNG creation
    WritePng(ofilename, image, 
     gfxHeader->inf_imgWidth, gfxHeader->inf_imgHeight,
     loc_palp->palette, loc_palp->colorsCount, loc_palp->tRNS_array, loc_palp->tRNS_count);

    //cleaning
    free(image);
}

struct gfx_palette createDefaultPalette(bool _256colors, bool transparency){
    struct gfx_palette pal = {0};
    if(transparency){
        pal.tRNS_count = 1;
        pal.tRNS_array[0] = 0;
    }
    pal.colorsCount = (_256colors ? 256 : 16);
    for(u16 i = 0; i < 256; i+= (_256colors ? 1 : 16)){
        pal.palette[i / (_256colors ? 1 : 16)].blue = i;
        pal.palette[i / (_256colors ? 1 : 16)].green = i;
        pal.palette[i / (_256colors ? 1 : 16)].red = i;
    }
    return pal;
}
