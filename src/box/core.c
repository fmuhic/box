#include "box/core.h"
#include <stdlib.h>

void* bx_alloc(size_t size) {
    if (size == 0) return NULL;
    return malloc(size);
}

void bx_free(void* ptr) {
    free(ptr);
}
