#pragma once
#include <stdint.h>
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;

#ifndef FSCAN_GFX_INFO_VEC_TYPEDEF
#define FSCAN_GFX_INFO_VEC_TYPEDEF 1
typedef gexdev_univec fscan_gfx_info_vec;
#endif

fscan_gfx_info_vec fscan_tiles_scan(struct fscan_files *files_stp);
