#pragma once
#include <stdlib.h>
#include <graphics/gfx.h>
#include <graphics/write_png.h>
#include <filescanning/filescanning.h>

int fscan_draw_gfx_using_gfx_info_test(char lev_filename[]){
    fscan_files fscan_files_obj = {0};
    fscan_gfx_info_vec gfx_info_vec = {0};
    gfx_graphic graphic = {0};

    fscan_files_init(&fscan_files_obj, lev_filename);
    gexdev_univec_init_capcity(&gfx_info_vec, 256, sizeof(fscan_gfx_info));

    // Scan the file for graphics
    fscan_tiles_scan(&fscan_files_obj, &gfx_info_vec);

    if(gfx_info_vec.size == 0){
        fscan_files_close(&fscan_files_obj);
        fscan_gfx_info_vec_close(&gfx_info_vec);
        return 1;
    }

    printf("Palette offset: %X\n", fscan_gfx_info_vec_at(&gfx_info_vec, 0)->palette_offset);

    // Draw a graphic
    fscan_draw_gfx_using_gfx_info(&fscan_files_obj, fscan_gfx_info_vec_at(&gfx_info_vec, 0), &graphic);
    if(graphic.bitmap == NULL){
        fscan_files_close(&fscan_files_obj);
        fscan_gfx_info_vec_close(&gfx_info_vec);
        return -1;
    }

    FILE *fp = fopen("test.png", "wb");
    gfx_write_png(fp, graphic.bitmap, graphic.width, graphic.height, graphic.palette);

    fscan_files_close(&fscan_files_obj);
    fscan_gfx_info_vec_close(&gfx_info_vec);
    return 0;
}