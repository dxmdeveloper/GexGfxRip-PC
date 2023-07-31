#pragma once
#include <stdint.h>
#include "../graphics/gfx.h"

struct fsmod_files;

void fsmod_tiles_scan(struct fsmod_files * filesStp, void *pass2cb,
                      void cb(void * clientp, const void *bitmap, const void *headerAndOpMap,
                              const struct gfx_palette *palette, uint16_t tileGfxId, uint16_t tileAnimFrameIndex));
