#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct gexdev_essentials_bitflag_arr_structure {
    size_t size;
    uint8_t *arr;
} gexdev_bitflag_arr;

/// @brief creates a new array of given size and initializes it with zeros
/// @retrun malloc result as boolean
bool gexdev_bitflag_arr_create(gexdev_bitflag_arr this[static 1], size_t size);

void gexdev_bitflag_arr_close(gexdev_bitflag_arr this[static 1]);

bool gexdev_bitflag_arr_get(const gexdev_bitflag_arr this[static 1], size_t index);

/// @return EXIT_SUCCESS or EXIT_FAILURE
int gexdev_bitflag_arr_set(gexdev_bitflag_arr this[static 1], size_t index, bool val);

