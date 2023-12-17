#include "binary_bool_array.h"

gexdev_binboolarr gexdev_binboolarr_create(size_t size) {
    gexdev_binboolarr arr = {size, calloc((size + (size % 8 ? 1 : 0)) / 8, sizeof(uint8_t))};
    return arr;
}

int gexdev_binboolarr_set(gexdev_binboolarr this[static 1], size_t index, bool val) {
    if(index >= this->size)
        return EXIT_FAILURE;

    uint8_t mask = 1 << (index % 8);
    if (val)
        this->arr[index / 8] |= mask;
    else
        this->arr[index / 8] &= ~mask;
    return EXIT_SUCCESS;
}

bool gexdev_binboolarr_get(const gexdev_binboolarr this[static 1], size_t index) {
    return this->arr[index / 8] & (1 << (index % 8));
}

void gexdev_binboolarr_close(gexdev_binboolarr this[static 1]) {
    free(this->arr);
}
