#include "Stack.h"
#include <stdio.h>

void Stack32_init(Stack32 *s, size_t size){
    if(!(s->stack = malloc(4 * size)))
        fprintf(stderr, "failed to initialize Stack32\n");
    s->size = size;
}

void Stack32_close(Stack32 *s){
    if(s->stack) free(s->stack);
}

int Stack32_push(Stack32 * s, uint32_t val){
    if(s->sp * 4 >= s->size) return EXIT_FAILURE;
    s->stack[s->sp] = val;
    s->sp++;
    return EXIT_SUCCESS;
}

uint32_t Stack32_pop(Stack32 * s){
    if(s->sp == 0) return UINT32_MAX;
    s->sp--;
    return s->stack[s->sp];
}