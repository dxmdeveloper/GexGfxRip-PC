#include "vector.h"

int gexdev_u32vec_init_size(gexdev_u32vec * vecp, size_t size){
    size_t cap = (size ? size : 1);
    vecp->v = calloc(cap, sizeof(uint32_t));
    if(!vecp->v) return EXIT_FAILURE;
    vecp->capacity = cap;
    vecp->size = size;
    return EXIT_SUCCESS;
}

int gexdev_u32vec_init_capcity(gexdev_u32vec * vecp, size_t capacity){
    if(!capacity) return EXIT_FAILURE;
    vecp->v = malloc(capacity * sizeof(uint32_t));
    if(!vecp->v) return EXIT_FAILURE;
    vecp->capacity = capacity;
    vecp->size = 0;
    return EXIT_SUCCESS;
}

void gexdev_u32vec_close(gexdev_u32vec * vecp){
    if(vecp->v) free(vecp->v);
}

int gexdev_u32vec_push_back(gexdev_u32vec * vecp, uint32_t val){
    if(vecp->size == vecp->capacity){
        vecp->v = realloc(vecp->v, vecp->capacity * 2 * 4);
        if(!vecp->v) return EXIT_FAILURE;
        vecp->capacity *= 2;
    }
    vecp->v[vecp->size] = val;
    vecp->size++;
    return EXIT_SUCCESS;
}

void gexdev_u32vec_pop_back(gexdev_u32vec * vecp){
    if(vecp->size == 0) return;
    vecp->size--;
}

void gexdev_u32vec_ascounter_inc(gexdev_u32vec * vecp, size_t index){
    if(vecp->size > index){
        vecp->v[index]++;
        for(size_t i = index+1; i < vecp->size; i++)
             vecp->v[i] = 0;
    }
    else for(size_t i = vecp->size; i <= index; i++){
        gexdev_u32vec_push_back(vecp, 0);
    }
}

int gexdev_uptrvec_init_size(gexdev_uptrvec *vecp, size_t size) {
    size_t cap = (size ? size : 1);
    vecp->v = calloc(cap, sizeof(void *));
    if(!vecp->v) return EXIT_FAILURE;
    vecp->capacity = cap;
    vecp->size = size;
    return EXIT_SUCCESS;
}

int gexdev_uptrvec_init_capcity(gexdev_uptrvec *vecp, size_t capacity) {
    if(!capacity) return EXIT_FAILURE;
    vecp->v = malloc(capacity * sizeof(void *));
    if(!vecp->v) return EXIT_FAILURE;
    vecp->capacity = capacity;
    vecp->size = 0;
    return EXIT_SUCCESS;
}

void gexdev_uptrvec_close(gexdev_uptrvec *vecp) {
    if(vecp->v) free(vecp->v);
}

int gexdev_uptrvec_push_back(gexdev_uptrvec *vecp, void * val) {
    if(vecp->size == vecp->capacity){
        vecp->v = realloc(vecp->v, vecp->capacity * 2 * 4);
        if(!vecp->v) return EXIT_FAILURE;
        vecp->capacity *= 2;
    }
    vecp->v[vecp->size] = val;
    vecp->size++;
    return EXIT_SUCCESS;
}

void gexdev_uptrvec_pop_back(gexdev_uptrvec *vecp) {
    if(vecp->size == 0) return;
    vecp->size--;
}
