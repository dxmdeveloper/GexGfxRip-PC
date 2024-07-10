#pragma once
#include <stdint.h>
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;
struct fscan_gfx_info_vec;

int fscan_tiles_scan(struct fscan_files *sf, struct fscan_gfx_info_vec *res_vec);
