#pragma once
#include <stdint.h>
#include "../graphics/gfx.h"

struct fscan_files;

void fscan_tiles_scan(struct fscan_files * filesStp, void *pass2cb,
                      void cb(void * clientp, const void *headers, const void *bitmap,
                              const struct gfx_palette *palette, uint16_t tileGfxId, uint16_t tileAnimFrameIndex));
