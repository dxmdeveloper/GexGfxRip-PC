#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;
struct fscan_gfx_info_vec;


// TODO: DOCUMENTATION
int fscan_obj_gfx_scan(struct fscan_files *sf, struct fscan_gfx_info_vec *res_vec);

int fscan_intro_obj_gfx_scan(struct fscan_files *sf, struct fscan_gfx_info_vec *res_vec);

int fscan_background_scan(struct fscan_files *sf, struct fscan_gfx_info_vec *res_vec);