#include <stdio.h>
#include <png.h>
#include <stdlib.h>
#include "helpers/basicdefs.h"
#include "filescanning/filescanning.h"
#include "graphics/write_png.h"
#include "graphics/gfx.h"

// mkdir / stat
#ifdef _WIN32
    #include <direct.h>
    #define MAKEDIR(x) _mkdir(x)
#else //POSIX
    #include <sys/stat.h>
    #define MAKEDIR(x) mkdir(x, 0755)
#endif

// STATIC DECLARATIONS:
void cb_onTileFound(void *clientp, const void *bitmap, const void *headerAndOpMap, const struct gfx_palette *palette, u16 tileGfxID, u16 tileAnimFrameI);
void cb_onObjGfxFound(void * clientp, const void *bitmap, const void *headerAndOpMap, const struct gfx_palette *palette, gexdev_u32vec * itervecp);

void printUsageHelp(){
    printf("USAGE: ."PATH_SEP"gexgfxrip [path to file]\n");
}

struct application_options {
    char * savePath;
};

//-------------------- Program Entry Point --------------------------
int main(int argc, char *argv[]) {
    struct fsmod_files fsmodFilesSt = {0};
    struct application_options options = {0};
    char odirname[17];
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
            
            // output directory name
            sprintf(odirname, "%s-rip/", ifilename);
            options.savePath = odirname;
            // Scan found file
            fsmod_files_init(&fsmodFilesSt, ifilename);
            if(fsmodFilesSt.tilesChunk.ptrsFp && fsmodFilesSt.mainChunk.ptrsFp)
                fsmod_tiles_scan(&fsmodFilesSt, &options, cb_onTileFound);
            fsmod_files_close(&fsmodFilesSt);
        }
    } else {
        if(fsmod_files_init(&fsmodFilesSt, argv[argc-1]) >= 0){
            // output directory name
            sprintf(odirname, "%s-rip/", argv[argc-1]);
            options.savePath = odirname;
            /*
            if(fsmodFilesSt.tilesChunk.ptrsFp && fsmodFilesSt.mainChunk.ptrsFp)
                fsmod_tiles_scan(&fsmodFilesSt, &options, cb_onTileFound);
                */
            if(fsmodFilesSt.mainChunk.ptrsFp)
                fsmod_obj_gfx_scan(&fsmodFilesSt, &options, cb_onObjGfxFound);
            fsmod_files_close(&fsmodFilesSt);
        }
    }
    return 0;     
}
//-------------------------------------------------------------------


// callback function for scan4Gfx
// TODO: output filename based on program argument
void cb_onTileFound(void *clientp, const void *bitmap, const void *headerAndOpMap, const struct gfx_palette *palette, u16 tileGfxID, u16 tileAnimFrameI){
    png_byte ** image = NULL;
    char filePath[PATH_MAX] = "\0";
    FILE * filep = NULL;
    struct gex_gfxHeader gfxHeader = {0};
    u32 realWidth = 0, realHeight = 0;
    struct application_options * appoptp = clientp;

    // infinite loop protection
    static int counter = 0;
    static char lastSavePath[PATH_MAX]; //! MOVE TO CLIENTP
    if(strcmp(lastSavePath, appoptp->savePath)){
        counter = 0;
        strncpy(lastSavePath, appoptp->savePath, PATH_MAX - 1);
    }
    counter++;
    if(counter > 20000){ dbg_errlog("FILES LIMIT REACHED\n"); exit(123);}
    // ----------------------------------------

    if(counter == 1){
        MAKEDIR(appoptp->savePath); // TODO: add more options for user
    }

    gfxHeader = gex_gfxHeader_parseAOB(headerAndOpMap);

    // Exception handling 
    if(palette == NULL) {
        fprintf(stderr, "Err: invalid gex color palette\n");
        return;
    }
    
    else if((gfxHeader.typeSignature & 1) && palette->colorsCount < 256){
        fprintf(stderr, "Err: color palette and graphic types mismatch\n");
        return;
    }
    
    // Image creation
    image = gfx_drawImgFromRaw(headerAndOpMap, bitmap);
    if(image == NULL) {
        dbg_errlog("DEBUG: failed to create %s", filePath);
        return;
    }
    
    // File opening
    snprintf(filePath, PATH_MAX-1, "%s%04X-%d.png", appoptp->savePath, tileGfxID, tileAnimFrameI);
    if((filep = fopen(filePath, "wb")) == NULL){
        fprintf(stderr, "Err: Cannot open file %s\n", filePath);
        free(image);
        return;
    }

    gfx_calcRealWidthAndHeight(&realWidth, &realHeight, headerAndOpMap+20);
    gfxHeader.inf_imgWidth = MAX(gfxHeader.inf_imgWidth, realWidth);
    gfxHeader.inf_imgHeight = MAX(gfxHeader.inf_imgHeight, realHeight);

    // PNG creation
    gfx_write_png(filep, image, 
    gfxHeader.inf_imgWidth, gfxHeader.inf_imgHeight, (gfxHeader.typeSignature & 2 ? NULL : palette));

    // Cleaning
    fclose(filep);
    free(image);
}


void cb_onObjGfxFound(void * clientp, const void *bitmap, const void *headerAndOpMap, const struct gfx_palette *palette, gexdev_u32vec * itervecp){
    // ! TEMPORARY IT'S JUST COPY OF cb_onTileFound
    png_byte ** image = NULL;
    char filePath[PATH_MAX] = "\0";
    FILE * filep = NULL;
    struct gex_gfxHeader gfxHeader = {0};
    u32 realWidth = 0, realHeight = 0;
    struct application_options * appoptp = clientp;

    // infinite loop protection
    static int counter = 0;
    static char lastSavePath[PATH_MAX]; //! MOVE TO CLIENTP
    if(strcmp(lastSavePath, appoptp->savePath)){
        counter = 0;
        strncpy(lastSavePath, appoptp->savePath, PATH_MAX - 1);
    }
    counter++;
    if(counter > 20000){ dbg_errlog("FILES LIMIT REACHED\n"); exit(123);}
    // ----------------------------------------

    if(counter == 1){
        MAKEDIR(appoptp->savePath); // TODO: add more options for user
    }

    gfxHeader = gex_gfxHeader_parseAOB(headerAndOpMap);

    // Exception handling 
    if(palette == NULL) {
        fprintf(stderr, "Err: invalid gex color palette\n");
        return;
    }
    
    else if((gfxHeader.typeSignature & 1) && palette->colorsCount < 256){
        fprintf(stderr, "Err: color palette and graphic types mismatch\n");
        return;
    }
    
    // Image creation
    image = gfx_drawImgFromRaw(headerAndOpMap, bitmap);
    if(image == NULL) {
        dbg_errlog("DEBUG: failed to create %s", filePath);
        return;
    }
    
    // File opening
    snprintf(filePath, PATH_MAX-70, "%s", appoptp->savePath);
    for(size_t i =0; i < itervecp->size; i++){
        char textToAppend[10];
        sprintf(textToAppend, "%u", itervecp->v[i]);
        if(i != itervecp->size-1) strcat(textToAppend, "-");
        strcat(filePath, textToAppend);
    }
    strcat(filePath, ".png");

    if((filep = fopen(filePath, "wb")) == NULL){
        fprintf(stderr, "Err: Cannot open file %s\n", filePath);
        free(image);
        return;
    }

    gfx_calcRealWidthAndHeight(&realWidth, &realHeight, headerAndOpMap+20);
    gfxHeader.inf_imgWidth = MAX(gfxHeader.inf_imgWidth, realWidth);
    gfxHeader.inf_imgHeight = MAX(gfxHeader.inf_imgHeight, realHeight);

    // PNG creation
    gfx_write_png(filep, image, 
    gfxHeader.inf_imgWidth, gfxHeader.inf_imgHeight, (gfxHeader.typeSignature & 2 ? NULL : palette));

    // Cleaning
    fclose(filep);
    free(image);
}