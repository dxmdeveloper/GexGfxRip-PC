#include <memory.h>
#include "vector.h"

int gexdev_u32vec_init_size(gexdev_u32vec *vecp, size_t size)
{
    size_t cap = (size ? size : 1);
    vecp->v = calloc(cap, sizeof(uint32_t));
    if (!vecp->v)
	return EXIT_FAILURE;
    vecp->capacity = cap;
    vecp->size = size;
    return EXIT_SUCCESS;
}

int gexdev_u32vec_init_capcity(gexdev_u32vec *vecp, size_t capacity)
{
    if (!capacity)
	return EXIT_FAILURE;
    vecp->v = malloc(capacity * sizeof(uint32_t));
    if (!vecp->v)
	return EXIT_FAILURE;
    vecp->capacity = capacity;
    vecp->size = 0;
    return EXIT_SUCCESS;
}

void gexdev_u32vec_close(gexdev_u32vec *vecp)
{
    if (vecp->v)
	free(vecp->v);
}

int gexdev_u32vec_push_back(gexdev_u32vec *vecp, uint32_t val)
{
    if (vecp->size == vecp->capacity) {
	void *new_v = realloc(vecp->v, vecp->capacity * 2 * 4);
	if (!new_v)
	    return EXIT_FAILURE;
	vecp->v = new_v;
	vecp->capacity *= 2;
    }
    vecp->v[vecp->size] = val;
    vecp->size++;
    return EXIT_SUCCESS;
}

void gexdev_u32vec_pop_back(gexdev_u32vec *vecp)
{
    if (vecp->size == 0)
	return;
    vecp->size--;
}

void gexdev_u32vec_ascounter_inc(gexdev_u32vec *vecp, size_t index)
{
    if (vecp->size > index) {
	vecp->v[index]++;
	for (size_t i = index + 1; i < vecp->size; i++)
	    vecp->v[i] = 0;
    } else
	for (size_t i = vecp->size; i <= index; i++) {
	    gexdev_u32vec_push_back(vecp, 0);
	}
}

int gexdev_univec_init_size(gexdev_univec *vecp, size_t size, size_t element_size)
{
    size_t cap = (size ? size : 1);
    vecp->v = calloc(cap, element_size);
    if (!vecp->v)
	return EXIT_FAILURE;
    vecp->capacity = cap;
    vecp->size = size;
    vecp->element_size = element_size;
    return EXIT_SUCCESS;
}

int gexdev_univec_init_capcity(gexdev_univec *vecp, size_t capacity, size_t element_size)
{
    if (!capacity)
	return EXIT_FAILURE;
    vecp->v = malloc(capacity * element_size);
    if (!vecp->v)
	return EXIT_FAILURE;
    vecp->capacity = capacity;
    vecp->size = 0;
    vecp->element_size = element_size;
    return EXIT_SUCCESS;
}

void gexdev_univec_close(gexdev_univec *vecp)
{
    if (vecp->v)
	free(vecp->v);
}

int gexdev_univec_push_back(gexdev_univec *vecp, const void *val)
{
    if (vecp->size == vecp->capacity) {
	void *new_v = realloc(vecp->v, vecp->capacity * 2 * vecp->element_size);
	if (!new_v)
	    return EXIT_FAILURE;
	vecp->v = new_v;
	vecp->capacity *= 2;
    }
    memcpy(vecp->v + vecp->size * vecp->element_size, val, vecp->element_size);
    vecp->size++;
    return EXIT_SUCCESS;
}
