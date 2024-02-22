#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "vector.h"

#define PTR_MAP_REGION_SIZE 4096

/** @brief Pointer map used instead of hash map (there are no collisions). 
  * In fact it's a big array of pointers that may be divided into memory regions allocated dynamically.
  * If max index is high can allocate extra memory regions. Max index can't exceed UINT32_MAX */
typedef struct gexdev_paged_map {
    uint32_t (*index_compute_cb)(const void *key);
    uint32_t max_index;
    size_t page_count;
    gexdev_u32vec inserted_ptr_indexes_vec;
    void ***pages;
} gexdev_paged_map;

/** @param index_compute_cb If NULL then the map will behave like ordinary array.
 *  @return EXIT_SUCCESS or EXIT_FAILURE */
int gexdev_paged_map_init(gexdev_paged_map *paged_map, uint32_t max_index, uint32_t (*index_compute_cb)(const void *key));

/** @brief Frees memory allocated by the structure. Does not free pointers it contains. */
void gexdev_paged_map_close_only_map(gexdev_paged_map *paged_map);

/** @brief Frees both the memory allocated by the structure and the pointers it contains.
  * CAUTION: If the pointers in the map were not allocated using malloc/calloc, a SEGFAULT may occur. */
void gexdev_paged_map_close_all(gexdev_paged_map *paged_map);

/** @brief Calls index_compute_cb to obtain actual index of a pointer then returns it.
    @return Desired pointer or NULL if it doesn't exist or value obtained from index_compute_cb is above max_index. */
void *gexdev_paged_map_get(gexdev_paged_map *paged_map, const void *key);

/** @brief Calls index_compute_cb to obtain index where pointer will be inserted.
  * @param uniq_ptr Pointer to be inserted. The structure takes ownership of memory allocated pointed by uniq_ptr.
  * @return EXIT_SUCCESS or EXIT_FAILURE if value obtained from index_compute_cb is above max_index. */
int gexdev_paged_map_set(gexdev_paged_map *paged_map, const void *key, void *uniq_ptr);