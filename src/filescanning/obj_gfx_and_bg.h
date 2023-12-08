#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;

size_t fscan_obj_gfx_scan(struct fscan_files *files_stp);

size_t fscan_intro_obj_gfx_scan(struct fscan_files *files_stp);

size_t fscan_background_scan(struct fscan_files *files_stp);