#pragma once
#include "../graphics/gfx.h"
#include "../essentials/vector.h"

struct fscan_files;
void fscan_obj_gfx_scan(struct fscan_files *filesStp, void *pass2cb,
			void cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				uint32_t iterations[4]));

void fscan_intro_obj_gfx_scan(struct fscan_files *filesStp, void *pass2cb,
			      void cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				      uint32_t iterations[4]));

void fscan_background_scan(struct fscan_files *files_stp, void *pass2cb,
			   void cb(void *clientp, const void *headers, const void *bitmap, const struct gfx_palette *palette,
				   uint32_t iterations[5]));