#include <stdio.h>
#include <png.h>
#include <stdlib.h>
#include <ctype.h>
#include <helpers/basicdefs.h>
#include <filescanning/filescanning.h>
#include <graphics/write_png.h>
#include <graphics/gfx.h>
#include <helpers/xpgetopt/xpgetopt.h>
#include <helpers/binary_parse.h>

#define FILE_COUNT_LIMIT 600000

// mkdir / stat
#ifdef _WIN32
#include <direct.h>
#define MAKEDIR(x) _mkdir(x)
#else //POSIX

#include <sys/stat.h>

#define MAKEDIR(x) mkdir(x, 0755)
#endif

#define GFX_CAT_ALL 100

// STATIC DECLARATIONS:
struct application_options
{
    char *save_path;
};

static int strcmp_ci(const char *str1, const char *str2);

static void print_fscan_gfx_info(const fscan_gfx_info *ginf, bool is_tile);

void print_fscan_gfx_info_vec(const fscan_gfx_info_vec *v);

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
    fscan_files sf = {0};
    struct application_options options = {0};
    char odirname[256];
    jmp_buf errbuf;
    int errno = 0;
    int verbose = 0;
    int type = GFX_CAT_ALL;

    // Application options
    struct xpoption options_table[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
    };

    switch (xpgetopt_long(argc, argv, "hvt:", options_table, NULL)) {
        case 'h':print_usage_info();
            return 0;
        case 'v':sf.option_verbose = true;
            verbose = 1;
            break;
        case '?':print_usage_info();
            return 1;
        case 't':
            if (strcmp_ci(xpoptarg, "all") == 0)
                type = GFX_CAT_ALL; // default
            else if (strcmp_ci(xpoptarg, "tiles") == 0)
                type = GFX_CAT_TILE;
            else if (strcmp_ci(xpoptarg, "objects") == 0)
                type = GFX_CAT_OBJ;
            else if (strcmp_ci(xpoptarg, "intro") == 0)
                type = GFX_CAT_INTRO_OBJ;
            else if (strcmp_ci(xpoptarg, "backgrounds") == 0)
                type = GFX_CAT_BACKGROUND;
            else {
                fprintf(stderr, "error: unknown type '%s'\n", xpoptarg);
                return 1;
            }
    }

    // setjmp error handling
    if ((errno = setjmp(errbuf))) {
        fprintf(stderr, "error while scanning file %i", errno);
        fscan_files_close(&sf);
        return -1;
    }

    fscan_gfx_info_vec results[GFX_CATEGORIES] = {0};

    // if no additional program arguments or asterisk
    if (argc == 1) {
//        char ifilename[11];
//        for (u8 fileI = 0; fileI < 255; fileI++) {
//            sprintf(ifilename, "GEX%03u.LEV", fileI);
//
//            // Test file availability
//            FILE *testFile = fopen(ifilename, "rb");
//            if (testFile == NULL)
//                continue;
//            fclose(testFile);
//
//            // output directory name
//            sprintf(odirname, "%s-rip/", ifilename);
//            options.save_path = odirname;
//            }
//        }
    } else {
        for (int i = xpoptind; i < argc; i++) {
            if (fscan_files_init(&sf, argv[xpoptind]) < 0) {
                dbg_errlog_va("error: failed to open file %s\n", argv[xpoptind]);
                continue;
            }
            // output directory name
            sprintf(odirname, "%s-rip/", argv[xpoptind]);
            options.save_path = odirname;

            for (int i = 0; i < GFX_CATEGORIES; i++) {
                if ((type != GFX_CAT_ALL && type != i)
                    || !fscan_files_is_chunk_existing(&sf, fscan_get_gfx_category_header_origin(i)))
                    continue;

                fscan_gfx_scan(&sf, &results[i], i);
                if (verbose)
                    print_fscan_gfx_info_vec(&results[i]);

                // Do something with the data
                // ...
                if(i == GFX_CAT_OBJ || i == GFX_CAT_INTRO_OBJ){

                }
                // ...

                fscan_scan_result_close(&results[i]);
            }
            fscan_files_close(&sf);
        }
    }
    return 0;
}
//-------------------------------------------------------------------

/// @return EXIT_SUCCESS or EXIT_FAILURE
// TODO: REMOVE THIS
inline static int draw_img_and_create_png(const void *headers, const void *bitmap, const struct gfx_palette *palette,
                                          const char *out_filename)
{
    FILE *fp = NULL;
    struct gex_gfxheader gfxHeader = {0};
    struct gfx_graphic graphic = {0};

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
    graphic = gfx_draw_img_from_raw(headers, bitmap);
    if (graphic.bitmap == NULL) {
        fprintf(stderr, "error: failed to create %s\n", out_filename);
        return EXIT_FAILURE;
    }

    // file open to write png
    fp = fopen(out_filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "error: failed to open and create %s\n", out_filename);
        gfx_graphic_close(&graphic);
        return EXIT_FAILURE;
    }

    // PNG write
    gfx_write_png(fp, graphic.bitmap, graphic.width, graphic.height, graphic.palette);

    if (fp)
        fclose(fp);
    gfx_graphic_close(&graphic);
    return EXIT_SUCCESS;
}

void print_fscan_gfx_info(const fscan_gfx_info *ginf, bool is_tile)
{
    if (!ginf) {
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
    if (ginf->ext_bmp_offsets && ginf->chunk_count > 0) {
        printf("ext_bmp_offsets: { ");
        for (size_t i = 0; i < ginf->chunk_count; i++) {
            printf("0x%08X ", ginf->ext_bmp_offsets[i]);
        }
        printf("}\n");
    }

    if (!is_tile)
        printf("iteration: [%u, %u, %u, %u]\n",
               ginf->iteration[3],
               ginf->iteration[2],
               ginf->iteration[1],
               ginf->iteration[0]);
    else
        printf("tileGfxID: 0x%04X (block: %u, anim: %u)\n",
               aob_read_LE_U16(&ginf->iteration[1]),
               ginf->iteration[0],
               ginf->iteration[3]);
}

int strcmp_ci(const char *str1, const char *str2)
{
    while (*str1 && *str2 && tolower(*str1) == tolower(*str2))
        str1++, str2++;
    return *str1 - *str2;
}

void print_fscan_gfx_info_vec(const fscan_gfx_info_vec *v)
{
    const bool is_tile = (v->gfx_category == GFX_CAT_TILE);
    for (size_t i = 0; i < v->base.size; i++)
        print_fscan_gfx_info(fscan_gfx_info_vec_at(v, i), is_tile);
}
