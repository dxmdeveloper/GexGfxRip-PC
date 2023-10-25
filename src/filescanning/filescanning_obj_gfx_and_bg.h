#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files_st;
void fscan_obj_gfx_scan(struct fscan_files_st *files_stp, void *pass2cb,
			void cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				uint32_t iterations[4], struct gfx_properties *gfx_props));

void fscan_intro_obj_gfx_scan(struct fscan_files_st *files_stp, void *pass2cb,
			      void cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				      uint32_t iterations[4], struct gfx_properties *gfx_props));

void fscan_background_scan(struct fscan_files_st *files_stp, void *pass2cb,
			   void cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				   uint32_t iterations[4], struct gfx_properties *gfx_props));