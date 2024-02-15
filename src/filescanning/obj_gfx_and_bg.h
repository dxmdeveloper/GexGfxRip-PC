#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;

#ifndef FSCAN_GFX_INFO_VEC_TYPEDEF
#define FSCAN_GFX_INFO_VEC_TYPEDEF 1
typedef gexdev_univec fscan_gfx_info_vec;
#endif

// TODO: DOCUMENTATION, TELL ABOUT CLOSING THE VECTOR
fscan_gfx_info_vec fscan_obj_gfx_scan(struct fscan_files *files_stp);

fscan_gfx_info_vec fscan_intro_obj_gfx_scan(struct fscan_files *files_stp);

fscan_gfx_info_vec fscan_background_scan(struct fscan_files *files_stp);