#pragma once
#include <stdint.h>
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;
typedef gexdev_univec fscan_gfx_info_vec;

fscan_gfx_info_vec fscan_tiles_scan(struct fscan_files *files_stp);
