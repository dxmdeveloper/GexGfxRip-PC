#pragma once
#include <stdlib.h>
#include <graphics/gfx.h>
#include <graphics/write_png.h>
#include <filescanning/filescanning.h>

static void print_fscan_gfx_info(const fscan_gfx_info *ginf, bool is_tile)
{
    if (!ginf) {
        printf("NULLPTR\n");
        return;
    }

    if (!is_tile)
        printf("iteration: [%u, %u, %u, %u]\n",
               ginf->iteration[3],
               ginf->iteration[2],
               ginf->iteration[1],
               ginf->iteration[0]);
    else
        printf("tileGfxID: 0x%04X (block: %u, anim: %u)\n",
               *(uint16_t*)&ginf->iteration[1],
               ginf->iteration[0],
               ginf->iteration[3]);

    printf("gfx_offset: 0x%08X\n", ginf->gfx_offset);
    printf("palette_offset: 0x%08X\n", ginf->palette_offset);
    printf("chunk_count: %u\n", ginf->chunk_count);
    printf("size: %u x %u\n", ginf->width, ginf->height);
    // gfx_props
    printf("gfx_props: \n");
    printf("\t .pos_x %X (%d)\n", ginf->gfx_props.pos_x, *(int16_t*)&ginf->gfx_props.pos_x);
    printf("\t .pos_y %X (%d)\n", ginf->gfx_props.pos_y, *(int16_t*)&ginf->gfx_props.pos_y);
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
    printf("\n");
}

int fscan_draw_gfx_using_gfx_info_test(char lev_filename[]){
    fscan_files fscan_files_obj = {0};
    fscan_gfx_info_vec gfx_info_vec = {0};
    gfx_graphic graphic = {0};

    fscan_files_init(&fscan_files_obj, lev_filename);
    gexdev_univec_init_capcity(&gfx_info_vec, 256, sizeof(fscan_gfx_info));

    // Scan the file for graphics
    fscan_obj_gfx_scan(&fscan_files_obj, &gfx_info_vec);

    if(gfx_info_vec.base.size == 0){
        fscan_files_close(&fscan_files_obj);
        fscan_gfx_info_vec_close(&gfx_info_vec);
        return 1;
    }

    for(size_t i = 0; i < gfx_info_vec.base.size; i++){
        print_fscan_gfx_info(fscan_gfx_info_vec_at(&gfx_info_vec, i), false);
    }

//    if(fscan_gfx_info_vec_at(&gfx_info_vec, 0)->width != 32 || fscan_gfx_info_vec_at(&gfx_info_vec, 0)->height != 32){
//        printf("Width and height are not 32\n");
//        fscan_files_close(&fscan_files_obj);
//        fscan_gfx_info_vec_close(&gfx_info_vec);
//        return -1;
//    }

    // Draw a graphic
    fscan_draw_gfx_using_gfx_info(&fscan_files_obj, fscan_gfx_info_vec_at(&gfx_info_vec, 96), 3, FCH_TYPE_MAIN, &graphic);
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