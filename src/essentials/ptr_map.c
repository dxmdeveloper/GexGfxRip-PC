#include "ptr_map.h"
#include <stdbool.h>

static inline void **
gexdev_ptr_map_get_ptr_to_ptr(gexdev_ptr_map * ptrmapp, uint32_t absolute_index, bool allocRegionIfNotPresent){
    if(absolute_index > ptrmapp->max_index) return NULL;
    void *** regionpp = &ptrmapp->mem_regions[absolute_index / PTR_MAP_REGION_SIZE];
    if(!*regionpp){
        if(!allocRegionIfNotPresent
        || !(*regionpp = calloc(PTR_MAP_REGION_SIZE, sizeof(void *)))) return NULL;
    }
    return &(*regionpp)[absolute_index % PTR_MAP_REGION_SIZE];
}

static uint32_t gexdev_ptr_map_default_cb(const void * key){
    return *((uint32_t*) key);
}

int gexdev_ptr_map_init(gexdev_ptr_map *ptrmapp, uint32_t max_index, uint32_t (*index_compute_cb)(const void *)) {
    ptrmapp->max_index = max_index;
    if(index_compute_cb) ptrmapp->index_compute_cb = index_compute_cb;
    else ptrmapp->index_compute_cb = gexdev_ptr_map_default_cb;
    
    ptrmapp->region_count = max_index / PTR_MAP_REGION_SIZE + 1;
    ptrmapp->mem_regions = calloc(ptrmapp->region_count, sizeof(void*));
    if(!ptrmapp->mem_regions) return EXIT_FAILURE;
    if(ptrmapp->region_count == 1) {
        if (!(ptrmapp->mem_regions[0] = calloc(PTR_MAP_REGION_SIZE, sizeof(void *)))) {
            gexdev_ptr_map_close_only_map(ptrmapp); return EXIT_FAILURE;
        }
    }
    if(gexdev_u32vec_init_capcity(&ptrmapp->inserted_ptr_indexes_vec, 128)) {
        gexdev_ptr_map_close_only_map(ptrmapp); return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void gexdev_ptr_map_close_only_map(gexdev_ptr_map *ptrmapp) {
    for(size_t i = 0; i < ptrmapp->region_count; i++)
        if(ptrmapp->mem_regions[i]) {
            free(ptrmapp->mem_regions[i]);
            ptrmapp->mem_regions[i] = NULL;
        }
    if(ptrmapp->mem_regions){
        free(ptrmapp->mem_regions);
        ptrmapp->mem_regions = NULL;
    }
    gexdev_u32vec_close(&ptrmapp->inserted_ptr_indexes_vec);
}

void gexdev_ptr_map_close_all(gexdev_ptr_map *ptrmapp) {
    for(size_t i = 0; i < ptrmapp->inserted_ptr_indexes_vec.size; i++){
        uint32_t index = ptrmapp->inserted_ptr_indexes_vec.v[i];
        void ** pp = gexdev_ptr_map_get_ptr_to_ptr(ptrmapp, index, false);
        if(pp && *pp) {free(*pp); *pp = NULL;}
    }
    gexdev_ptr_map_close_only_map(ptrmapp);
}

void *gexdev_ptr_map_get(gexdev_ptr_map * ptrmapp, const void *key) {
    uint32_t abs_index = ptrmapp->index_compute_cb(key);
    void ** pp = gexdev_ptr_map_get_ptr_to_ptr(ptrmapp, abs_index, false);
    return (pp ? *pp : NULL);
}

int gexdev_ptr_map_set(gexdev_ptr_map * ptrmapp, const void *key, void *pointer) {
    uint32_t abs_index = ptrmapp->index_compute_cb(key);
    void ** pp = gexdev_ptr_map_get_ptr_to_ptr(ptrmapp, abs_index, true);
    if(!pp) return EXIT_FAILURE;
    *pp = pointer;
    return EXIT_SUCCESS;
}
