#include "stdio.h"
#include "box/hmap.h"
#include <stdint.h>

static uint64_t hash_i32(int32_t key) {
    return (uint64_t)key * 11400714819323198485ULL; // Simple mixing hash
}

static bool eq_i32(int32_t a, int32_t b) {
    return a == b;
}

BX_HMAP_DECLARE(int32_t, float, i32f32)
BX_HMAP_SOURCE(int32_t, float, i32f32, hash_i32, eq_i32)

int main()
{
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);
    
    int32_t key = 50;
    bx_hmap_i32f32_insert(&map, key, 5.0f);
    
    printf("Table | size = %zu, bucket_count = %zu\n", map.size, map.bucket_count);
    printf("Table | k: %i, v: %f\n", key, *bx_hmap_i32f32_get(&map, key));
    
    bx_hmap_i32f32_drop(&map);
    
    return 0;
}
