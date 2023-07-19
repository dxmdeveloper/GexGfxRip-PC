#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "Vector.h"

#define PTR_MAP_REGION_SIZE 4096

/** @brief Pointer map used instead of hash map (there are no collisions). 
  * In fact it's a big array of pointers that may be divided into memory regions allocated dynamically.
  * If max index is high can allocate extra memory regions. Max index can't exceed UINT32_MAX */
typedef struct gexdev_ptr_map {
    uint32_t (*index_compute_cb)(const void * key);
    uint32_t max_index;
    size_t region_count;
    gexdev_u32vec inserted_ptr_indexes_vec;
    void *** mem_regions;
} gexdev_ptr_map;

/** @param index_compute_cb If NULL then the map will behave like ordinary array.
 *  @return EXIT_SUCCESS or EXIT_FAILURE */
int gexdev_ptr_map_init(gexdev_ptr_map * ptrmapp, uint32_t max_index, uint32_t (*index_compute_cb)(const void * key));

/** @brief Frees memory allocated by the structure. Does not free pointers it contains. */
void gexdev_ptr_map_close_only_map(gexdev_ptr_map * ptrmapp);

/** @brief Frees both the memory allocated by the structure and the pointers it contains.
  * CAUTION: If the pointers in the map were not allocated using malloc/calloc, a SEGFAULT may occur. */
void gexdev_ptr_map_close_all(gexdev_ptr_map * ptrmapp);

/** @brief Calls index_compute_cb to obtain actual index of a pointer then returns it.
    @return Desired pointer or NULL if it doesn't exist or value obtained from index_compute_cb is above max_index. */
void* gexdev_ptr_map_get(gexdev_ptr_map * ptrmapp, const void * key);

/** @brief Calls index_compute_cb to obtain index where pointer will be inserted.
  * @return EXIT_SUCCESS or EXIT_FAILURE if value obtained from index_compute_cb is above max_index. */
int gexdev_ptr_map_set(gexdev_ptr_map * ptrmapp, const void * key, void * pointer);