#pragma once

#include <stddef.h>
#include <stdint.h>

void* bx_alloc(size_t size);
void bx_free(void* ptr);
int bx_pop_count_64(uint64_t x);
size_t bx_next_pow2(uint64_t x);
