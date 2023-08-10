#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fsmod_files;
void fsmod_obj_gfx_scan(struct fsmod_files * filesStp, void *pass2cb,
                      void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                              const struct gfx_palette *palette, gexdev_u32vec * itervecp));
