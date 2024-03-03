#include "bitflag_array.h"

bool gexdev_bitflag_arr_create(gexdev_bitflag_arr this[static 1], size_t size)
{
    this->size = size;
    void *arr = calloc((size + (size % 8 ? 1 : 0)) / 8, sizeof(uint8_t));
    return (this->arr = arr);
}

int gexdev_bitflag_arr_set(gexdev_bitflag_arr this[static 1], size_t index, bool val)
{
    if (index >= this->size)
        return EXIT_FAILURE;

    uint8_t mask = 1 << (index % 8);
    if (val)
        this->arr[index / 8] |= mask;
    else
        this->arr[index / 8] &= ~mask;
    return EXIT_SUCCESS;
}

bool gexdev_bitflag_arr_get(const gexdev_bitflag_arr this[static 1], size_t index)
{
    return this->arr[index / 8] & (1 << (index % 8));
}

void gexdev_bitflag_arr_close(gexdev_bitflag_arr this[static 1])
{
    if (this->arr)
        free(this->arr);
    this->arr = NULL;
    this->size = 0;
}
