#pragma once
#include <stdlib.h>
#include <stdint.h>

typedef struct gexdev_stack32_struct {
    size_t size;
    size_t sp;

    uint32_t *stack;

} gexdev_stack32;

void gexdev_stack32_init(gexdev_stack32 *stackp, size_t size);
void gexdev_stack32_close(gexdev_stack32 *stackp);

/** @return EXIT_SUCCESS or EXIT_FAILURE */
int gexdev_stack32_push(gexdev_stack32 *stackp, uint32_t val);

/** @return poped value. If stack pointer (sp) equals 0, function returns UINT32_MAX */
uint32_t gexdev_stack32_pop(gexdev_stack32 *stackp);