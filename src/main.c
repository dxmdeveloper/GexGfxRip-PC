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
struct application_options {
    char * save_path;
};

struct onfound_pack {
    struct application_options * app_options;
    bool is_tile_dir_created;
    bool is_obj_gfx_dir_created;
    bool is_intro_dir_created;
    bool is_bg_dir_created;
};

static void cb_on_tile_found(void *clientp, const void *headers, const void *bitmap,
                             const struct gfx_palette *palette, u16 tileGfxID, u16 tileAnimFrameI);
static void cb_on_obj_gfx_found(void * clientp, const void *headers, const void *bitmap,
                                const struct gfx_palette *palette, u32 iterations[static 4]);
static void cb_on_intro_obj_found(void * clientp, const void *headers, const void *bitmap,
                                  const struct gfx_palette *palette, u32 iterations[static 4]);
static void cb_on_backgrounds_found(void * clientp, const void *headers, const void *bitmap,
                                    const struct gfx_palette *palette, u32 iterations[static 4]);

void printUsageHelp(){
    printf("USAGE: ."PATH_SEP"gexgfxrip [path to file]\n");
}


//-------------------- Program Entry Point --------------------------
int main(int argc, char *argv[]) {
    struct fscan_files fscan_files_st = {0};
    struct application_options options = {0};
    char odirname[256];
    jmp_buf errbuf;
    jmp_buf * errbufp; int errno = 0;


    if((errno = setjmp(errbuf))){
        fprintf(stderr, "error while scanning file %i", errno);
        fscan_files_close(&fscan_files_st);
        return -1;
    }

    // if no additional program arguments or
    if(argc == 1 || strcmp(argv[argc-1], "*") == 0){
        char ifilename[11];
        for(u8 fileI = 0; fileI < 255; fileI++){
            struct onfound_pack pack = {&options, 0};
            sprintf(ifilename, "GEX%03u.LEV", fileI);

            // Test file availability
            FILE* testFile = fopen(ifilename, "rb");
            if(testFile == NULL) continue;
            fclose(testFile);
            
            // output directory name
            sprintf(odirname, "%s-rip/", ifilename);
            options.save_path = odirname;

            if(fscan_files_st.tile_chunk.ptrs_fp && fscan_files_st.main_chunk.ptrs_fp)
                fscan_tiles_scan(&fscan_files_st, &pack, cb_on_tile_found);
            if(fscan_files_st.main_chunk.ptrs_fp)
                fscan_obj_gfx_scan(&fscan_files_st, &pack, cb_on_obj_gfx_found);
            if(fscan_files_st.intro_chunk.ptrs_fp)
                fscan_intro_obj_gfx_scan(&fscan_files_st, &pack, cb_on_intro_obj_found);
            if(fscan_files_st.bg_chunk.ptrs_fp)
                fscan_background_scan(&fscan_files_st, &pack, cb_on_backgrounds_found);

            fscan_files_close(&fscan_files_st);
        }
    } else {
        if(fscan_files_init(&fscan_files_st, argv[argc - 1]) >= 0){
            // output directory name
            struct onfound_pack pack = {&options, 0};
            sprintf(odirname, "%s-rip/", argv[argc-1]);
            options.save_path = odirname;

            if(fscan_files_st.tile_chunk.ptrs_fp && fscan_files_st.main_chunk.ptrs_fp)
                fscan_tiles_scan(&fscan_files_st, &pack, cb_on_tile_found);
            if(fscan_files_st.main_chunk.ptrs_fp)
                fscan_obj_gfx_scan(&fscan_files_st, &pack, cb_on_obj_gfx_found);
            if(fscan_files_st.intro_chunk.ptrs_fp)
                fscan_intro_obj_gfx_scan(&fscan_files_st, &pack, cb_on_intro_obj_found);
            if(fscan_files_st.bg_chunk.ptrs_fp)
                fscan_background_scan(&fscan_files_st, &pack, cb_on_backgrounds_found);

            fscan_files_close(&fscan_files_st);
        }
    }
    return 0;     
}
//-------------------------------------------------------------------

/// @return EXIT_SUCCESS or EXIT_FAILURE
inline static int draw_img_and_create_png(const void *headers, const void *bitmap, const struct gfx_palette *palette,
                                           const char *out_filename){
    png_byte ** image = NULL;
    u32 realWidth = 0, realHeight = 0;
    FILE * fp = NULL;
    struct gex_gfxheader gfxHeader = {0};

    gfxHeader = gex_gfxheader_parse_aob(headers);

    // palette validation
    if(gfxHeader.typeSignature & 2) palette = NULL;

    if((gfxHeader.typeSignature & 2) == 0 && !palette){
        fprintf(stderr, "error: palette is missing\n");
        return EXIT_FAILURE;
    }
    if((gfxHeader.typeSignature & 1) && palette->colorsCount < 256){
        fprintf(stderr, "error: color palette and graphic types mismatch\n");
        return EXIT_FAILURE;
    }

    // image creation
    image = gfx_draw_img_from_raw(headers, bitmap);
    if(image == NULL) {
        fprintf(stderr, "error: failed to create %s\n", out_filename);
        return EXIT_FAILURE;
    }

    // file open to write png
    fp = fopen(out_filename, "wb");
    if(fp == NULL){
        fprintf(stderr, "error: failed to open and create %s\n", out_filename);
        free(image);
        return EXIT_FAILURE;
    }

    gfx_calc_real_width_and_height(&realWidth, &realHeight, headers+20);
    // PNG write
    gfx_write_png(fp, image,realWidth, realHeight,  palette);

    if(fp) fclose(fp);
    free(image);
    return EXIT_SUCCESS;
}

// TODO: output filename based on program argument
static void cb_on_tile_found(void *clientp, const void *headers, const void *bitmap,
                             const struct gfx_palette *palette, u16 tileGfxID, u16 tileAnimFrameI){
    char filePath[PATH_MAX] = "\0";
    struct onfound_pack * packp = clientp;

    // infinite loop protection
    static int counter = 0;
    if(++counter > 30000){ dbg_errlog("FILE COUNT LIMIT REACHED\n"); exit(123);}
    // ----------------------------------------

    if(!packp->is_tile_dir_created){
        MAKEDIR(packp->app_options->save_path); // TODO: add more options
        snprintf(filePath, PATH_MAX, "%s/tiles", packp->app_options->save_path);
        MAKEDIR(filePath);
    }
    snprintf(filePath, PATH_MAX, "%s/tiles/%04X-%u.png", packp->app_options->save_path, tileGfxID, tileAnimFrameI);
    draw_img_and_create_png(headers, bitmap, palette, filePath);
}


inline static void on_obj_gfx_found_body(void * clientp, const void *headers, const void *bitmap,
                                         const struct gfx_palette *palette, uint iterations[4],
                                         const bool * isdircreatedflagp, const char * subdir){
    char filePath[PATH_MAX] = "\0";
    struct onfound_pack * packp = clientp;

    // infinite loop protection
    static int counter = 0;
    if(++counter > 30000){ dbg_errlog("FILE COUNT LIMIT REACHED\n"); exit(123);}
    // ----------------------------------------

    if(!*isdircreatedflagp){
        MAKEDIR(packp->app_options->save_path); // TODO: add more options
        snprintf(filePath, PATH_MAX, "%s/%s", packp->app_options->save_path, subdir);
        MAKEDIR(filePath);
    }
    snprintf(filePath, PATH_MAX, "%s/%s/%u-%u-%u-%u.png",
             packp->app_options->save_path, subdir, iterations[0], iterations[1], iterations[2], iterations[3]);
    draw_img_and_create_png(headers, bitmap, palette, filePath);
}

static void cb_on_obj_gfx_found(void * clientp, const void *headers, const void *bitmap,
                                const struct gfx_palette *palette, u32 iterations[4]){
    on_obj_gfx_found_body(clientp, headers, bitmap, palette, iterations,
                          &((struct onfound_pack *)clientp)->is_obj_gfx_dir_created, "objects");
}

void cb_on_intro_obj_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                           u32 iterations[4]) {
    on_obj_gfx_found_body(clientp, headers, bitmap, palette, iterations,
                          &((struct onfound_pack *)clientp)->is_intro_dir_created, "intro");
}

void cb_on_backgrounds_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                             u32 iterations[4]) {
    on_obj_gfx_found_body(clientp, headers, bitmap, palette, iterations,
                          &((struct onfound_pack *)clientp)->is_bg_dir_created, "backgrounds");
}