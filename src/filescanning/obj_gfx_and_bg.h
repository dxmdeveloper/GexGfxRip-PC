#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"
#include "filescanning.h"

struct fscan_files_st;
size_t fscan_obj_gfx_scan(fscan_files *files_stp, void *pass2cb,
			  onfound_cb_t cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				uint32_t iterations[4], struct gfx_properties *gfx_props));

size_t fscan_intro_obj_gfx_scan(fscan_files *files_stp, void *pass2cb,
				onfound_cb_t cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				      uint32_t iterations[4], struct gfx_properties *gfx_props));

size_t fscan_background_scan(fscan_files *files_stp, void *pass2cb,
			     onfound_cb_t cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				   uint32_t iterations[4], struct gfx_properties *gfx_props));