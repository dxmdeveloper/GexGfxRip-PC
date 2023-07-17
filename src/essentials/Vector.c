#include "Vector.h"

int gexdev_u32vec_init_size(gexdev_u32vec * vecp, size_t size){
    size_t cap = (size ? size : 1);
    vecp->container = malloc(cap * sizeof(uint32_t));
    if(!vecp->container) return EXIT_FAILURE;
    vecp->capacity = cap;
    vecp->size = size;
    return EXIT_SUCCESS;
}

int gexdev_u32vec_init_capcity(gexdev_u32vec * vecp, size_t capacity){
    if(!capacity) return EXIT_FAILURE;
    vecp->container = malloc(capacity * sizeof(uint32_t));
    if(!vecp->container) return EXIT_FAILURE;
    vecp->capacity = capacity;
    vecp->size = 0;
    return EXIT_SUCCESS;
}

void gexdev_u32vec_close(gexdev_u32vec * vecp){
    if(vecp->container) free(vecp->container);
}

int gexdev_u32vec_push_back(gexdev_u32vec * vecp, uint32_t val){
    if(vecp->size == vecp->capacity){
        vecp->container = realloc(vecp->container, vecp->capacity * 2 * 4);
        if(!vecp->container) return EXIT_FAILURE;
        vecp->capacity *= 2;
    }
    vecp->container[vecp->size] = val;
    vecp->size++;
    return EXIT_SUCCESS;
}

void gexdev_u32vec_pop_back(gexdev_u32vec * vecp){
    if(vecp->size == 0) return;
    vecp->size--;
}

void gexdev_u32vec_ascounter_inc(gexdev_u32vec * vecp, size_t index){
    if(vecp->size > index){
        vecp->container[index]++;
        for(size_t i = index+1; i < vecp->size; i++)
             vecp->container[i] = 0;
    }
    else for(size_t i = vecp->size; i <= index; i++){
        gexdev_u32vec_push_back(vecp, 0);
    }
}