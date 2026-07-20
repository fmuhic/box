#include "box/core.h"
#include <stdlib.h>

void* bx_alloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    return malloc(size);
}

// Mirrors bx_alloc: a zero-byte request frees and yields NULL.
void* bx_realloc(void* ptr, size_t size)
{
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

void bx_free(void* ptr)
{
    free(ptr);
}

int bx_pop_count_64(uint64_t x)
{
#if defined(_MSC_VER)
    return (int)__popcnt64(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (int)((x * 0x0101010101010101ULL) >> 56);
#endif
}

size_t bx_next_pow2(uint64_t x)
{
    if (x <= 1)
    {
        return 1;
    }

#if defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse64(&index, x - 1);
    return 1ULL << (index + 1);
#elif defined(__GNUC__) || defined(__clang__)
    return 1ULL << (64 - __builtin_clzll(x - 1));
#else
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
#endif
}
