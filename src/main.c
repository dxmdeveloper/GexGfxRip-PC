#include <stdio.h>
#include <png.h>
#include <stdlib.h>
#include "helpers/basicdefs.h"
#include "filescanning/filescanning.h"
#include "graphics/write_png.h"
#include "graphics/gfx.h"


// STATIC DECLARATIONS:
void onfoundClbFunc(void *clientp, const void *bitmap, const void *headerAndOpMap, const struct gfx_palette *palette, const char filename[]);
void printUsageHelp(){
    printf("USAGE: ."PATH_SEP"gexgfxrip [path to file]\n");
}


//-------------------- Program Entry Point --------------------------
int main(int argc, char *argv[]) {
    struct fsmod_files fsmodFilesSt = {0};
    jmp_buf errbuf;
    jmp_buf * errbufp; int errno = 0;

    if((errno = setjmp(errbuf))){
        fprintf(stderr, "error while scanning file %i", errno);
        fsmod_files_close(&fsmodFilesSt);
        return -1;
    }

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
            fsmod_files_init(&fsmodFilesSt, ifilename);
            fsmod_tiles_scan(&fsmodFilesSt, NULL, onfoundClbFunc);
            fsmod_files_close(&fsmodFilesSt);
        }
    } else {
        if(fsmod_files_init(&fsmodFilesSt, argv[argc-1]) >= 0){
            fsmod_tiles_scan(&fsmodFilesSt, NULL, onfoundClbFunc);
            fsmod_files_close(&fsmodFilesSt);
        }
    }
    return 0;     
}
//-------------------------------------------------------------------



// callback function for scan4Gfx
// TODO: output filename based on program argument
void onfoundClbFunc(void *clientp, const void *bitmap, const void *headerAndOpMap, const struct gfx_palette *palette, const char ofilename[]){
    png_byte ** image = NULL;
    FILE * filep = NULL;
    struct gex_gfxHeader gfxHeader = {0};
    u32 realWidth = 0, realHeight = 0;

    // infinite loop protection
    static int counter = 0;
    counter++;
    if(counter > 8000){ dbg_errlog("FILES LIMIT REACHED\n"); exit(123);}

    gfxHeader = gex_gfxHeader_parseAOB(headerAndOpMap);

    // Exception handling 
    if(palette == NULL) {
        fprintf(stderr, "Err: invalid gex color palette\n");
        return;
    }
    
    if(ofilename == NULL){
        fprintf(stderr, "Err: ofilename is nullptr (main.c:onfoundClbFunc)");
        return;
    }
    else if((gfxHeader.typeSignature & 1) && palette->colorsCount < 256){
        fprintf(stderr, "Err: color palette and graphic types mismatch\n");
        return;
    }
    
    // Image creation
    image = gfx_drawImgFromRaw(headerAndOpMap, bitmap);
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

    gfx_calcRealWidthAndHeight(&realWidth, &realHeight, headerAndOpMap+20);
    gfxHeader.inf_imgWidth = MAX(gfxHeader.inf_imgWidth, realWidth);
    gfxHeader.inf_imgHeight = MAX(gfxHeader.inf_imgHeight, realHeight);

    // PNG creation
    WritePng(filep, image, 
    gfxHeader.inf_imgWidth, gfxHeader.inf_imgHeight, palette);

    // Cleaning
    fclose(filep);
    free(image);
}
