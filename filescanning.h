#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "gfx.h"

typedef void (*scan_foundCallback_t)(void*, const struct gfx_palette*, const char[]);

/// @brief scans file for gex graphics files
/// @param filename path to file to read
/// @param foundCallback callback function which is executed on every found graphic. 
/// takes 2 arguments: pointer to found graphic and pointer to assigned color palette (may be null ptr).
void scan4Gfx(char filename[], scan_foundCallback_t);


/// @brief function scanning memory for u32 value
/// @param endPtr end of the scanning range. The offset is excluded from the scan.
/// @param ORMask logical OR mask. 0 by default
/// @param matchVal searched value.
/// @return offset of found value. null if not found.
uintptr_t findU32(void *startPtr, void *endPtr, uint32_t ORMask, uint32_t matchVal);