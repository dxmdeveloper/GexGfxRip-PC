#include "stack.h"
#include <stdio.h>

void gexdev_stack32_init(gexdev_stack32 *stackp, size_t size){
    if(!(stackp->stack = malloc(4 * size)))
        fprintf(stderr, "failed to initialize gexdev_stack32\n");
    stackp->size = size;
}

void gexdev_stack32_close(gexdev_stack32 *stackp){
    if(stackp->stack) free(stackp->stack);
}

int gexdev_stack32_push(gexdev_stack32 * stackp, uint32_t val){
    if(stackp->sp * 4 >= stackp->size) return EXIT_FAILURE;
    stackp->stack[stackp->sp] = val;
    stackp->sp++;
    return EXIT_SUCCESS;
}

uint32_t gexdev_stack32_pop(gexdev_stack32 * stackp){
    if(stackp->sp == 0) return UINT32_MAX;
    stackp->sp--;
    return stackp->stack[stackp->sp];
}