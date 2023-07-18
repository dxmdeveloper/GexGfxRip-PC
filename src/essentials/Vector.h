#pragma once
#include <stdlib.h>
#include <stdint.h>

typedef struct _gexdev_tools_u32vector_structure {
    size_t size;
    size_t capacity;
    uint32_t *v;
} gexdev_u32vec;

///@return EXIT_SUCCESS or EXIT_FAILURE
int gexdev_u32vec_init_size(gexdev_u32vec * vecp, size_t size);

///@return EXIT_SUCCESS or EXIT_FAILURE
int gexdev_u32vec_init_capcity(gexdev_u32vec * vecp, size_t capacity);

void gexdev_u32vec_close(gexdev_u32vec * vecp);

///@return EXIT_SUCCESS or EXIT_FAILURE
int gexdev_u32vec_push_back(gexdev_u32vec * vecp, uint32_t val);

void gexdev_u32vec_pop_back(gexdev_u32vec * vecp);

/** @brief increases value by 1. Every next element is set to 0.
  * If element with given index doesn't exist then the vector gets expanded.
  * In that case no value is increased. New elements are 0 initialized. */
void gexdev_u32vec_ascounter_inc(gexdev_u32vec * vecp, size_t index);

