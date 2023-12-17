#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct gexdev_essentials_binboolarr_structure {
    size_t size;
    uint8_t *arr;
} gexdev_binboolarr;

/// @brief creates a new array of given size and initializes it with zeros
gexdev_binboolarr gexdev_binboolarr_create(size_t size);

void gexdev_binboolarr_close(gexdev_binboolarr this[static 1]);

bool gexdev_binboolarr_get(const gexdev_binboolarr this[static 1], size_t index);

/// @return EXIT_SUCCESS or EXIT_FAILURE
int gexdev_binboolarr_set(gexdev_binboolarr this[static 1], size_t index, bool val);

