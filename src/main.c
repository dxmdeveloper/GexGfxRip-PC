#include <stdio.h>
#include <png.h>
#include <stdlib.h>
#include <ctype.h>
#include "helpers/basicdefs.h"
#include "filescanning/filescanning.h"
#include "graphics/write_png.h"
#include "graphics/gfx.h"
#include "helpers/xpgetopt/xpgetopt.h"

#define FILE_COUNT_LIMIT 600000

// mkdir / stat
#ifdef _WIN32
#include <direct.h>
#define MAKEDIR(x) _mkdir(x)
#else //POSIX

#include <sys/stat.h>

#define MAKEDIR(x) mkdir(x, 0755)
#endif

// STATIC DECLARATIONS:
struct application_options
{
    char *save_path;
};

struct onfound_pack
{
    struct application_options *app_options;
    bool is_tile_dir_created;
    bool is_obj_gfx_dir_created;
    bool is_intro_dir_created;
    bool is_bg_dir_created;
};

enum GFX_TYPE_ENUM
{
    TYPE_ALL,
    TYPE_TILES,
    TYPE_OBJECTS,
    TYPE_INTRO,
    TYPE_BACKGROUNDS,
};

static void cb_on_tile_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                             u16 tileGfxID,
                             u16 tileAnimFrameI);

static void
cb_on_obj_gfx_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                    u32 iterations[static 4], struct gfx_properties *);

static void
cb_on_intro_obj_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                      u32 iterations[static 4], struct gfx_properties *);

static void
cb_on_backgrounds_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                        u32 iterations[static 4], struct gfx_properties *);

static int strcmp_ci(const char *str1, const char *str2)
{
    while (*str1 && *str2 && tolower(*str1) == tolower(*str2))
        str1++, str2++;
    return *str1 - *str2;
}

static void print_fscan_gfx_info(const fscan_gfx_info *ginf){
    if(!ginf){
        printf("NULLPTR\n");
        return;
    }

    printf("gfx_offset: 0x%08X\n", ginf->gfx_offset);
    printf("palette_offset: 0x%08X\n", ginf->palette_offset);
    printf("chunk_count: %u\n", ginf->chunk_count);
    // gfx_props
    printf("gfx_props: \n");
    printf("\t .pos_x %u\n", ginf->gfx_props.pos_x);
    printf("\t .pos_y %u\n", ginf->gfx_props.pos_y);
    printf("\t .is_semi_transparent %u\n", ginf->gfx_props.is_semi_transparent);
    printf("\t .is_flipped_horizontally %u\n", ginf->gfx_props.is_flipped_horizontally);
    printf("\t .is_flipped_vertically %u\n", ginf->gfx_props.is_flipped_vertically);
    // ext_bmp_offsets
    if(ginf->ext_bmp_offsets && ginf->chunk_count > 0) {
        printf("ext_bmp_offsets: { ");
        for (size_t i = 0; i < ginf->chunk_count; i++) {
            printf("0x%08X ", ginf->ext_bmp_offsets[i]);
        }
        printf("}\n");
    }

    printf("iteration: [%u, %u, %u, %u]\n", ginf->iteration[3], ginf->iteration[2], ginf->iteration[1], ginf->iteration[0]);
}

static void print_usage_info()
{
    printf("Usage: gexgfxrip [OPTION]... [FILE]\n");
    printf("Extracts graphics from Gex (PC) game files.\n");
    printf("  -h, --help\t\t\tPrint this help message and exit\n");
    printf("  -v, --verbose\t\t\tVerbose output\n");
    printf("  -t, --type=TYPE\t\t\tType of graphics to extract\n");
    printf("\t\t\t TYPE is 'all' (default), 'tiles', 'objects', 'intro' or 'backgrounds'\n");
}

//-------------------- Program Entry Point --------------------------
int main(int argc, char *argv[])
{
    fscan_files fscan_files_obj = {0};
    struct application_options options = {0};
    char odirname[256];
    jmp_buf errbuf;
    int errno = 0;
    int verbose = 0;
    int type = TYPE_ALL;

    // Application options
    struct xpoption options_table[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
    };

    switch (xpgetopt_long(argc, argv, "hvt:", options_table, NULL)) {
        case 'h':print_usage_info();
            return 0;
        case 'v':
            fscan_files_obj.option_verbose = true;
            verbose = 1;
            break;
        case '?':print_usage_info();
            return 1;
        case 't':
            if (strcmp_ci(xpoptarg, "all") == 0)
                type = TYPE_ALL; // default
            else if (strcmp_ci(xpoptarg, "tiles") == 0)
                type = TYPE_TILES;
            else if (strcmp_ci(xpoptarg, "objects") == 0)
                type = TYPE_OBJECTS;
            else if (strcmp_ci(xpoptarg, "intro") == 0)
                type = TYPE_INTRO;
            else if (strcmp_ci(xpoptarg, "backgrounds") == 0)
                type = TYPE_BACKGROUNDS;
            else {
                fprintf(stderr, "error: unknown type '%s'\n", xpoptarg);
                return 1;
            }
    }

    // setjmp error handling
    if ((errno = setjmp(errbuf))) {
        fprintf(stderr, "error while scanning file %i", errno);
        fscan_files_close(&fscan_files_obj);
        return -1;
    }

    fscan_gfx_info_vec tiles = {0};
    fscan_gfx_info_vec objects = {0};
    fscan_gfx_info_vec intro_objects = {0};
    fscan_gfx_info_vec backgrounds = {0};

    // if no additional program arguments or asterisk
    if (argc == 1) {
        char ifilename[11];
        for (u8 fileI = 0; fileI < 255; fileI++) {
            struct onfound_pack pack = {&options, 0};
            sprintf(ifilename, "GEX%03u.LEV", fileI);

            // Test file availability
            FILE *testFile = fopen(ifilename, "rb");
            if (testFile == NULL)
                continue;
            fclose(testFile);

            // output directory name
            sprintf(odirname, "%s-rip/", ifilename);
            options.save_path = odirname;

            if (fscan_files_init(&fscan_files_obj, ifilename) >= 0) {
                if ((type == TYPE_ALL || type == TYPE_TILES) && fscan_files_obj.tile_bmp_chunk.fp &&
                    fscan_files_obj.main_chunk.fp){}
                    //tiles = fscan_tiles_scan(&fscan_files_obj);
                if ((type == TYPE_ALL || type == TYPE_OBJECTS) && fscan_files_obj.main_chunk.fp)
                    objects = fscan_obj_gfx_scan(&fscan_files_obj);
                if ((type == TYPE_ALL || type == TYPE_INTRO) && fscan_files_obj.intro_chunk.fp)
                    intro_objects = fscan_intro_obj_gfx_scan(&fscan_files_obj);
                if ((type == TYPE_ALL || type == TYPE_BACKGROUNDS) && fscan_files_obj.bg_chunk.fp)
                    backgrounds = fscan_background_scan(&fscan_files_obj);

                // Do something with the data
                // ...

                fscan_scan_result_close(&tiles);
                fscan_scan_result_close(&objects);
                fscan_scan_result_close(&intro_objects);
                fscan_scan_result_close(&backgrounds);

                fscan_files_close(&fscan_files_obj);
            }
        }
    } else {
        for (int i = xpoptind; i < argc; i++) {
            if (fscan_files_init(&fscan_files_obj, argv[xpoptind]) >= 0) {
                // output directory name
                struct onfound_pack pack = {&options, 0};
                sprintf(odirname, "%s-rip/", argv[xpoptind]);
                options.save_path = odirname;

                // TODO: move to separate function
                //
                //////////////////////////////////////////
                if ((type == TYPE_ALL || type == TYPE_TILES) && fscan_files_obj.tile_bmp_chunk.fp &&
                    fscan_files_obj.main_chunk.fp){}
                    tiles = fscan_tiles_scan(&fscan_files_obj);
                    if(verbose){
                        for (size_t ii = 0; ii < tiles.size; ii++) {
                            print_fscan_gfx_info(fscan_gfx_info_vec_at(&tiles, ii));
                            printf("\n");
                        }
                    }
                if ((type == TYPE_ALL || type == TYPE_OBJECTS) && fscan_files_obj.main_chunk.fp) {
                    objects = fscan_obj_gfx_scan(&fscan_files_obj);
                    if (verbose){
                        for (size_t ii = 0; ii < objects.size; ii++) {
                            print_fscan_gfx_info(fscan_gfx_info_vec_at(&objects, ii));
                            printf("\n");
                        }
                    }
                }
                if ((type == TYPE_ALL || type == TYPE_INTRO) && fscan_files_obj.intro_chunk.fp)
                    intro_objects = fscan_intro_obj_gfx_scan(&fscan_files_obj);
                if ((type == TYPE_ALL || type == TYPE_BACKGROUNDS) && fscan_files_obj.bg_chunk.fp)
                    backgrounds = fscan_background_scan(&fscan_files_obj);

                // print all objects
                for (size_t ii = 0; ii < objects.size; ii++) {
                    print_fscan_gfx_info(fscan_gfx_info_vec_at(&objects, ii));
                    printf("\n");
                }

                // Do something with the data
                // ...

                fscan_scan_result_close(&tiles);
                fscan_scan_result_close(&objects);
                fscan_scan_result_close(&intro_objects);
                fscan_scan_result_close(&backgrounds);

                fscan_files_close(&fscan_files_obj);
            } else {
                fprintf(stderr, "error: failed to open file %s\n", argv[xpoptind]);
            }
        }
    }
    return 0;
}
//-------------------------------------------------------------------

/// @return EXIT_SUCCESS or EXIT_FAILURE
inline static int draw_img_and_create_png(const void *headers, const void *bitmap, const struct gfx_palette *palette,
                                          const char *out_filename)
{
    png_byte **image = NULL;
    u32 realWidth = 0, realHeight = 0;
    FILE *fp = NULL;
    struct gex_gfxheader gfxHeader = {0};

    gfxHeader = gex_gfxheader_parse_aob(headers);

    // palette validation
    if (gfxHeader.type_signature & 2)
        palette = NULL;

    if ((gfxHeader.type_signature & 2) == 0 && !palette) {
        fprintf(stderr, "error: palette is missing\n");
        return EXIT_FAILURE;
    }
    if ((gfxHeader.type_signature & 1) && palette->colors_cnt < 256) {
        fprintf(stderr, "error: color palette and graphic types mismatch\n");
        return EXIT_FAILURE;
    }

    // image creation
    image = gfx_draw_img_from_raw(headers, bitmap);
    if (image == NULL) {
        fprintf(stderr, "error: failed to create %s\n", out_filename);
        return EXIT_FAILURE;
    }

    // file open to write png
    fp = fopen(out_filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "error: failed to open and create %s\n", out_filename);
        free(image);
        return EXIT_FAILURE;
    }

    gfx_calc_real_width_and_height(&realWidth, &realHeight, headers + 20);
    // PNG write
    gfx_write_png(fp, image, realWidth, realHeight, palette);

    if (fp)
        fclose(fp);
    free(image);
    return EXIT_SUCCESS;
}

// TODO: output filename based on program argument
static void cb_on_tile_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                             u16 tileGfxID,
                             u16 tileAnimFrameI)
{
    char filePath[PATH_MAX] = "\0";
    struct onfound_pack *packp = clientp;

    // infinite loop protection
    static int counter = 0;
    if (++counter > FILE_COUNT_LIMIT) {
        dbg_errlog("FILE COUNT LIMIT REACHED\n");
        exit(123);
    }
    // ----------------------------------------

    if (!packp->is_tile_dir_created) {
        MAKEDIR(packp->app_options->save_path); // TODO: add more options
        snprintf(filePath, PATH_MAX, "%s/tiles", packp->app_options->save_path);
        MAKEDIR(filePath);
        packp->is_tile_dir_created = true;
    }
    snprintf(filePath, PATH_MAX, "%s/tiles/%04X-%u.png", packp->app_options->save_path, tileGfxID, tileAnimFrameI);
    draw_img_and_create_png(headers, bitmap, palette, filePath);
}

inline static void
on_gfx_found_body(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                  uint iterations[4], bool *isdircreatedflagp, const char *subdir, const char *filename_format)
{
    char filePath[PATH_MAX] = "\0";
    char fformat[50] = "%s/%s/";
    struct onfound_pack *packp = clientp;

    // infinite loop protection
    static int counter = 0;
    if (++counter > FILE_COUNT_LIMIT) {
        dbg_errlog("FILE COUNT LIMIT REACHED\n");
        exit(123);
    }
    // ----------------------------------------

    if (!*isdircreatedflagp) {
        MAKEDIR(packp->app_options->save_path); // TODO: add more options
        snprintf(filePath, PATH_MAX, "%s/%s", packp->app_options->save_path, subdir);
        MAKEDIR(filePath);
        *isdircreatedflagp = true;
    }
    strncat(fformat, filename_format, 44);
    snprintf(filePath, PATH_MAX, fformat, packp->app_options->save_path, subdir, iterations[0], iterations[1],
             iterations[2],
             iterations[3]);
    draw_img_and_create_png(headers, bitmap, palette, filePath);
}

static void
cb_on_obj_gfx_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                    u32 iterations[4], struct gfx_properties *gfx_props)
{
    on_gfx_found_body(clientp, headers, bitmap, palette, iterations,
                      &((struct onfound_pack *) clientp)->is_obj_gfx_dir_created, "objects",
                      "%u-%u-%u-%u.png");
}

void cb_on_intro_obj_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                           u32 iterations[4],
                           struct gfx_properties *gfx_props)
{
    on_gfx_found_body(clientp, headers, bitmap, palette, iterations,
                      &((struct onfound_pack *) clientp)->is_intro_dir_created, "intro",
                      "%u-%u-%u-%u.png");
}

void cb_on_backgrounds_found(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
                             u32 iterations[4],
                             struct gfx_properties *gfx_props)
{
    on_gfx_found_body(clientp, headers, bitmap, palette, iterations,
                      &((struct onfound_pack *) clientp)->is_bg_dir_created, "backgrounds",
                      "%u-%u-%u-%u.png");
}