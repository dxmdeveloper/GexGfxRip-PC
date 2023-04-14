#include <stdio.h>
#include <png.h>
#include <stdlib.h>
#include "basicdefs.h"
#include "filescanning.h"
#include "write_png.h"
#include "gfx.h"


// STATIC DECLARATIONS:
struct gfx_palette createDefaultPalette(bool _256colors, bool transparency);
void onfoundClbFunc(void *gfx, const struct gfx_palette *palette, const char ofilename[]);
void printUsageHelp(){
    printf("USAGE: ."PATH_SEP"gexgfxrip [path to file]\n");
}

// INMUTABLE (only one assigment)
// TODO: REMOVE THIS
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
            fsmod_scan4Gfx(ifilename, onfoundClbFunc);
        }
    } else {
        fsmod_scan4Gfx(argv[argc-1], onfoundClbFunc);
    }
    return 0;     
}
//-------------------------------------------------------------------



// callback function for scan4Gfx
// TODO: output filename based on program argument
// TODO: Reimplement
void onfoundClbFunc(void *gfx, const struct gfx_palette *palette, const char ofilename[]){
    void **image = NULL;
    FILE * filep = NULL;
    struct gex_gfxHeader *gfxHeader = gfx;

    // Exception handling 
    if(palette == NULL) {
        fprintf(stderr, "Err: invalid gex color palette\n");
        if(gfxHeader->typeSignature & 1) palette = &const_grayscalePal256;
        else palette = &const_grayscalePal16;
    }
    if(ofilename == NULL){
        fprintf(stderr, "Err: ofilename is nullptr (main.c:onfoundClbFunc)");
        return;
    }
    else if((gfxHeader->typeSignature & 1) && palette->colorsCount < 256){
        fprintf(stderr, "Err: color palette and graphic types mismatch\n");
        return;
    }
    
    // Image creation
    //image = gfx_drawImgFromRaw(gfx);
    if(image == NULL) {
        dbg_errlog("DEBUG: failed to create %s", ofilename);
        return;
    }
    
    // File opening
    if((filep = fopen(ofilename, "wb")) == NULL){
        fprintf(stderr, "Err: Cannot open file %s", ofilename);
        free(image);
        return;
    }

    // PNG creation
    WritePng(ofilename, image, 
     gfxHeader->inf_imgWidth, gfxHeader->inf_imgHeight, palette);

    // Cleaning
    fclose(filep);
    free(image);
}

// TODO: REMOVE THIS
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
