#pragma once
#include "../essentials/vector.h"
#include <stdlib.h>
#include <setjmp.h>


// TODO: make macros and functions for error handling

typedef struct disposable_object_structure
{
    void *ptr;
    void (*free_func)(void *);
} gexdev_disposable_obj;

typedef struct error_handler_stucture
{
    jmp_buf *errbufp;
    gexdev_univec disposable_objects;
    gexdev_univec func_name_chain;
    int errcode;
} gexdev_errh;