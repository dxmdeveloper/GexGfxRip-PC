#include "paged_map.h"
#include <stdbool.h>

static inline void **p_gexdev_ptr_map_get_ptr_to_ptr(gexdev_paged_map *ptrmapp, uint32_t absolute_index, bool allocRegionIfNotPresent)
{
    if (absolute_index > ptrmapp->max_index)
	return NULL;
    void ***regionpp = &ptrmapp->pages[absolute_index / PTR_MAP_REGION_SIZE];
    if (!*regionpp) {
	if (!allocRegionIfNotPresent || !(*regionpp = calloc(PTR_MAP_REGION_SIZE, sizeof(void *))))
	    return NULL;
    }
    return &(*regionpp)[absolute_index % PTR_MAP_REGION_SIZE];
}

static uint32_t p_gexdev_ptr_map_default_cb(const void *key)
{
    return *((uint32_t *)key);
}

int gexdev_paged_map_init(gexdev_paged_map *paged_map, uint32_t max_index, uint32_t (*index_compute_cb)(const void *))
{
    paged_map->max_index = max_index;
    if (index_compute_cb)
        paged_map->index_compute_cb = index_compute_cb;
    else
        paged_map->index_compute_cb = p_gexdev_ptr_map_default_cb;

    paged_map->page_count = max_index / PTR_MAP_REGION_SIZE + 1;
    paged_map->pages = calloc(paged_map->page_count, sizeof(void *));
    if (!paged_map->pages)
	return EXIT_FAILURE;
    if (paged_map->page_count == 1) {
	if (!(paged_map->pages[0] = calloc(PTR_MAP_REGION_SIZE, sizeof(void *)))) {
        gexdev_paged_map_close_only_map(paged_map);
	    return EXIT_FAILURE;
	}
    }
    if (gexdev_u32vec_init_capcity(&paged_map->inserted_ptr_indexes_vec, 128)) {
        gexdev_paged_map_close_only_map(paged_map);
	return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void gexdev_paged_map_close_only_map(gexdev_paged_map *paged_map)
{
    for (size_t i = 0; i < paged_map->page_count; i++)
	if (paged_map->pages[i]) {
	    free(paged_map->pages[i]);
        paged_map->pages[i] = NULL;
	}
    if (paged_map->pages) {
	free(paged_map->pages);
        paged_map->pages = NULL;
    }
    gexdev_u32vec_close(&paged_map->inserted_ptr_indexes_vec);
}

void gexdev_paged_map_close_all(gexdev_paged_map *paged_map)
{
    for (size_t i = 0; i < paged_map->inserted_ptr_indexes_vec.size; i++) {
	uint32_t index = paged_map->inserted_ptr_indexes_vec.v[i];
	void **pp = p_gexdev_ptr_map_get_ptr_to_ptr(paged_map, index, false);
	if (pp && *pp) {
	    free(*pp);
	    *pp = NULL;
	}
    }
    gexdev_paged_map_close_only_map(paged_map);
}

void *gexdev_paged_map_get(gexdev_paged_map *paged_map, const void *key)
{
    uint32_t abs_index = paged_map->index_compute_cb(key);
    void **pp = p_gexdev_ptr_map_get_ptr_to_ptr(paged_map, abs_index, false);
    return (pp ? *pp : NULL);
}

int gexdev_paged_map_set(gexdev_paged_map *paged_map, const void *key, void *uniq_ptr)
{
    uint32_t abs_index = paged_map->index_compute_cb(key);
    void **pp = p_gexdev_ptr_map_get_ptr_to_ptr(paged_map, abs_index, true);
    if (!pp)
	return EXIT_FAILURE;
    *pp = uniq_ptr;
    return EXIT_SUCCESS;
}
