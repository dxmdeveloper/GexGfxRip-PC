#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;
void fscan_obj_gfx_scan(struct fscan_files * filesStp, void *pass2cb,
                        void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                                const struct gfx_palette *palette, gexdev_u32vec * itervecp));

void fscan_intro_obj_gfx_scan(struct fscan_files * filesStp, void *pass2cb,
                              void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                                      const struct gfx_palette *palette, gexdev_u32vec * itervecp));

void fscan_background_scan(struct fscan_files * filesStp, void *pass2cb,
                           void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                                   const struct gfx_palette *palette, gexdev_u32vec * itervecp));