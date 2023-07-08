#pragma once
#include <stdlib.h>
#include <stdint.h>

typedef struct Stack32_struct {
    size_t size;
    size_t sp;

    uint32_t * stack;

    int additionalInfo_level;
} Stack32;

void Stack32_init(Stack32 *stackp, size_t size);
void Stack32_close(Stack32 *stackp);

/** @return EXIT_SUCCESS or EXIT_FAILURE */
int Stack32_push(Stack32 *stackp, uint32_t val);

/** @return poped value. If stack pointer (sp) equals 0, function returns UINT32_MAX */
uint32_t Stack32_pop(Stack32 *stackp);