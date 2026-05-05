#pragma once

#include <stddef.h>
#include <stdint.h>

void* bx_alloc(size_t size);
void  bx_free(void* ptr);
