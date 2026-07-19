#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "box/core.h"
#include "box/hmap.h"

static uint64_t hash_i32(int32_t key)
{
    uint64_t x = (uint64_t)key;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}

static bool eq_i32(int32_t a, int32_t b)
{
    return a == b;
}

BX_HMAP_DECLARE(int32_t, float, i32f32, hash_i32, eq_i32)

void test_hmap_pow2_logic(void)
{
    printf("Running: test_hmap_pow2_logic\n");
    assert(bx_next_pow2(0) == 1);
    assert(bx_next_pow2(1) == 1);
    assert(bx_next_pow2(7) == 8);
    assert(bx_next_pow2(8) == 8);
    assert(bx_next_pow2(9) == 16);
}

void test_hmap_basic_ops(void)
{
    printf("Running: test_hmap_basic_ops\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    bx_hmap_i32f32_insert(&map, 10, 100.5f);
    bx_hmap_i32f32_insert(&map, 20, 200.5f);
    assert(map.base.size == 2);

    float* v1 = bx_hmap_i32f32_get(&map, 10);
    assert(v1 != NULL && *v1 == 100.5f);

    bx_hmap_i32f32_insert(&map, 10, 500.0f);
    assert(map.base.size == 2);
    assert(*bx_hmap_i32f32_get(&map, 10) == 500.0f);

    assert(bx_hmap_i32f32_get(&map, 99) == NULL);

    bx_hmap_i32f32_drop(&map);
}

void test_hmap_collisions_and_erasure(void)
{
    printf("Running: test_hmap_collisions_and_erasure\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    bx_hmap_i32f32_reserve(&map, 4);

    for (int i = 0; i < 6; i++)
    {
        bx_hmap_i32f32_insert(&map, i, (float)i * 1.1f);
    }

    for (int i = 0; i < 6; i++)
    {
        float* val = bx_hmap_i32f32_get(&map, i);
        assert(val != NULL);
    }

    // Test Backward-Shift Erasure
    // Erasing an element in a collision chain should move others back
    bx_hmap_i32f32_erase(&map, 2);
    assert(map.base.size == 5);
    assert(bx_hmap_i32f32_get(&map, 2) == NULL);

    // Ensure the chain wasn't broken for elements after the erased one
    assert(bx_hmap_i32f32_get(&map, 3) != NULL);
    assert(bx_hmap_i32f32_get(&map, 5) != NULL);

    bx_hmap_i32f32_drop(&map);
}

void test_hmap_stress_resize(void)
{
    printf("Running: test_hmap_stress_resize\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    int count = 2000;
    for (int i = 0; i < count; i++)
    {
        bx_hmap_i32f32_insert(&map, i, (float)i);
    }

    assert(bx_hmap_i32f32_size(&map) == (uint32_t)count);
    assert(bx_hmap_i32f32_bucket_count(&map) > (uint32_t)count);

    for (int i = 0; i < count; i++)
    {
        float* v = bx_hmap_i32f32_get(&map, i);
        if (v == NULL || *v != (float)i)
        {
            fprintf(stderr, "Failure at key %d\n", i);
            assert(false);
        }
    }

    for (int i = 0; i < count / 2; i++)
    {
        bx_hmap_i32f32_erase(&map, i);
    }
    assert(bx_hmap_i32f32_size(&map) == (uint32_t)count / 2);

    bx_hmap_i32f32_drop(&map);
}

void test_hmap_wrap_around(void)
{
    printf("Running: test_hmap_wrap_around\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    bx_hmap_i32f32_reserve(&map, 4);
    uint32_t count = bx_hmap_i32f32_bucket_count(&map);

    // Find a key that hashes to the very last bucket
    int32_t last_key = 0;
    while ((hash_i32(last_key) & (count - 1)) != (count - 1))
    {
        last_key++;
    }

    // Insert it and another key that should collide and wrap to index 0
    bx_hmap_i32f32_insert(&map, last_key, 999.0f);

    int32_t wrap_key = last_key + 1;
    while ((hash_i32(wrap_key) & (count - 1)) != (count - 1))
    {
        wrap_key++;
    }
    // This key also wants to be at the last index, but will be pushed to index 0
    bx_hmap_i32f32_insert(&map, wrap_key, 888.0f);

    // Verify both can be retrieved correctly across the wrap boundary
    assert(*bx_hmap_i32f32_get(&map, last_key) == 999.0f);
    assert(*bx_hmap_i32f32_get(&map, wrap_key) == 888.0f);

    // Erase the first one and ensure the second one shifts back to the end correctly
    bx_hmap_i32f32_erase(&map, last_key);
    assert(*bx_hmap_i32f32_get(&map, wrap_key) == 888.0f);

    bx_hmap_i32f32_drop(&map);
}

void test_hmap_update_duplicate(void)
{
    printf("Running: test_hmap_update_duplicate\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    bx_hmap_i32f32_insert(&map, 5, 1.0f);
    bx_hmap_i32f32_insert(&map, 5, 2.0f);
    bx_hmap_i32f32_insert(&map, 5, 3.0f);

    assert(map.base.size == 1);
    assert(*bx_hmap_i32f32_get(&map, 5) == 3.0f);

    bx_hmap_i32f32_drop(&map);
}

static void test_hmap_minihash_collision(void)
{
    printf("Running: test_hmap_minihash_collision\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    // We need two keys that have different values but produce
    // the same bits 24-29 in their hash.
    // We can just brute force two keys that collide on the mini_hash bits.
    int32_t k1 = 1;
    uint8_t target_mini = (uint8_t)((hash_i32(k1) >> 24) & 0x3fU);

    int32_t k2 = k1 + 1;
    while ((uint8_t)((hash_i32(k2) >> 24) & 0x3fU) != target_mini)
    {
        k2++;
    }

    bx_hmap_i32f32_insert(&map, k1, 10.0f);
    bx_hmap_i32f32_insert(&map, k2, 20.0f);

    // Verify the mini_hash filter didn't confuse k1 for k2
    assert(*bx_hmap_i32f32_get(&map, k1) == 10.0f);
    assert(*bx_hmap_i32f32_get(&map, k2) == 20.0f);

    bx_hmap_i32f32_drop(&map);
}

static void test_hmap_deep_backward_shift(void)
{
    printf("Running: test_deep_backward_shift\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);
    bx_hmap_i32f32_reserve(&map, 16);

    // Insert several keys that all want to go to the same bucket
    // This creates a long "poor" chain.
    int32_t keys[5];
    int found = 0;
    for (int32_t i = 0; found < 5; i++)
    {
        if ((hash_i32(i) & (map.base.bucket_count - 1)) == 5)
        {
            keys[found++] = i;
            bx_hmap_i32f32_insert(&map, i, (float)i);
        }
    }

    // Erase the very first key in the collision chain
    bx_hmap_i32f32_erase(&map, keys[0]);

    // Ensure everyone else shifted back and is still findable
    for (int i = 1; i < 5; i++)
    {
        float* val = bx_hmap_i32f32_get(&map, keys[i]);
        assert(val != NULL && *val == (float)keys[i]);
    }

    bx_hmap_i32f32_drop(&map);
}

static void test_hmap_empty_map_queries(void)
{
    printf("Running: test_hmap_empty_map_queries\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    // Getting/Erasing from a map with 0 buckets should not crash
    assert(bx_hmap_i32f32_get(&map, 10) == NULL);
    bx_hmap_i32f32_erase(&map, 10);

    assert(map.base.size == 0);
    bx_hmap_i32f32_drop(&map);
}

typedef struct vec3
{
    float x, y, z;
} vec3;

typedef struct Entity
{
    const char* name;
    vec3 position;
} Entity;

static uint64_t hash_vec3(vec3 v)
{
    uint32_t ux, uy, uz;
    memcpy(&ux, &v.x, 4);
    memcpy(&uy, &v.y, 4);
    memcpy(&uz, &v.z, 4);
    return (uint64_t)ux ^ ((uint64_t)uy << 16) ^ ((uint64_t)uz << 32);
}

static bool eq_vec3(vec3 a, vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static uint64_t hash_str(const char* s)
{
    uint64_t hash = 14695981039346656037ULL;
    while (*s)
    {
        hash ^= (uint64_t)(unsigned char)*s++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool eq_str(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

BX_HMAP_DECLARE(int32_t, vec3, i32v3, hash_i32, eq_i32)
BX_HMAP_DECLARE(const char*, Entity, str_ent, hash_str, eq_str)

void test_hamp_complex_structures(void)
{
    printf("Running: test_hamp_complex_structures\n");

    // Entity id -> position map
    bx_hmap_i32v3 id_to_pos;
    bx_hmap_i32v3_init(&id_to_pos);

    int32_t entity_id = 1024;
    vec3 target_pos = { 10.0f, 20.0f, 30.0f };

    bx_hmap_i32v3_insert(&id_to_pos, entity_id, target_pos);

    vec3* found_pos = bx_hmap_i32v3_get(&id_to_pos, entity_id);
    assert(found_pos != NULL);
    assert(found_pos->x == 10.0f && found_pos->y == 20.0f && found_pos->z == 30.0f);

    bx_hmap_i32v3_drop(&id_to_pos);

    // --- Name -> Entity map
    bx_hmap_str_ent name_to_ent;
    bx_hmap_str_ent_init(&name_to_ent);

    // Initial Entity
    Entity npc = { .name = "Merchant", .position = { 5.0f, 0.0f, 5.0f } };
    bx_hmap_str_ent_insert(&name_to_ent, npc.name, npc);

    Entity* found_npc = bx_hmap_str_ent_get(&name_to_ent, "Merchant");
    assert(found_npc != NULL);
    assert(found_npc->position.x == 5.0f);
    assert(strcmp(found_npc->name, "Merchant") == 0);

    // Move the NPC and update the map
    npc.position.z = 50.0f;
    bx_hmap_str_ent_insert(&name_to_ent, "Merchant", npc);
    assert(bx_hmap_str_ent_get(&name_to_ent, "Merchant")->position.z == 50.0f);

    bx_hmap_str_ent_erase(&name_to_ent, "Merchant");
    assert(bx_hmap_str_ent_get(&name_to_ent, "Merchant") == NULL);

    bx_hmap_str_ent_drop(&name_to_ent);
}

void test_hmap_init_capacity(void)
{
    printf("Running: test_hmap_init_capacity\n");
    bx_hmap_i32f32 map;

    // capacity counts elements: 100 / 0.80 + 4 = 129, rounded up to 256 buckets
    bx_hmap_i32f32_init_capacity(&map, 100);
    assert(bx_hmap_i32f32_bucket_count(&map) == 256);
    assert(bx_hmap_i32f32_size(&map) == 0);

    // Filling to the requested capacity must not rehash
    for (int32_t i = 0; i < 100; i++)
    {
        bx_hmap_i32f32_insert(&map, i, (float)i);
    }
    assert(bx_hmap_i32f32_size(&map) == 100);
    assert(bx_hmap_i32f32_bucket_count(&map) == 256);
    assert(*bx_hmap_i32f32_get(&map, 42) == 42.0f);

    bx_hmap_i32f32_drop(&map);
}

void test_hmap_clear_reuses_buckets(void)
{
    printf("Running: test_hmap_clear_reuses_buckets\n");
    bx_hmap_i32f32 map;
    bx_hmap_i32f32_init(&map);

    for (int32_t i = 0; i < 50; i++)
    {
        bx_hmap_i32f32_insert(&map, i, (float)i);
    }
    uint32_t buckets = bx_hmap_i32f32_bucket_count(&map);

    bx_hmap_i32f32_clear(&map);
    assert(bx_hmap_i32f32_size(&map) == 0);
    assert(bx_hmap_i32f32_bucket_count(&map) == buckets); // allocation kept
    assert(bx_hmap_i32f32_get(&map, 10) == NULL);
    assert(!bx_hmap_i32f32_contains(&map, 10));

    // Reinserting after a clear must behave exactly as on a fresh map.
    // If clear dropped the meta[bucket_count] probe guard, this is where it shows.
    for (int32_t i = 0; i < 50; i++)
    {
        bx_hmap_i32f32_insert(&map, i + 1000, (float)i);
    }
    assert(bx_hmap_i32f32_size(&map) == 50);
    for (int32_t i = 0; i < 50; i++)
    {
        assert(bx_hmap_i32f32_contains(&map, i + 1000));
        assert(*bx_hmap_i32f32_get(&map, i + 1000) == (float)i);
        assert(!bx_hmap_i32f32_contains(&map, i));
    }

    // Clearing an untouched map is a no-op, not a crash
    bx_hmap_i32f32 fresh;
    bx_hmap_i32f32_init(&fresh);
    bx_hmap_i32f32_clear(&fresh);
    assert(bx_hmap_i32f32_size(&fresh) == 0);
    bx_hmap_i32f32_drop(&fresh);

    bx_hmap_i32f32_drop(&map);
}

// 1 KB value: the whole entry is far past the 256-byte cap that used to exist
typedef struct BigPayload
{
    uint8_t bytes[1024];
} BigPayload;

BX_HMAP_DECLARE(int32_t, BigPayload, i32big, hash_i32, eq_i32)

static void fill_payload(BigPayload* p, int32_t key)
{
    for (size_t i = 0; i < sizeof(p->bytes); i++)
    {
        p->bytes[i] = (uint8_t)(key * 7 + (int32_t)i);
    }
}

static bool payload_intact(const BigPayload* p, int32_t key)
{
    for (size_t i = 0; i < sizeof(p->bytes); i++)
    {
        if (p->bytes[i] != (uint8_t)(key * 7 + (int32_t)i))
        {
            return false;
        }
    }
    return true;
}

void test_hmap_large_entries(void)
{
    printf("Running: test_hmap_large_entries\n");
    assert(sizeof(bx_hmap_i32big_entry) > 1024);

    bx_hmap_i32big map;
    bx_hmap_i32big_init(&map);

    // Start dense so inserts collide, displace, and force at least one rehash
    bx_hmap_i32big_reserve(&map, 4);
    uint32_t first_buckets = bx_hmap_i32big_bucket_count(&map);

    const int32_t count = 200;
    for (int32_t i = 0; i < count; i++)
    {
        BigPayload p;
        fill_payload(&p, i);
        bx_hmap_i32big_insert(&map, i, p);
    }
    assert(bx_hmap_i32big_size(&map) == (uint32_t)count);
    assert(bx_hmap_i32big_bucket_count(&map) > first_buckets); // rehashed

    // Every byte must survive displacement, rehashing and the swap scratch
    for (int32_t i = 0; i < count; i++)
    {
        BigPayload* got = bx_hmap_i32big_get(&map, i);
        assert(got != NULL);
        assert(payload_intact(got, i));
    }

    // Backward-shift deletion with large entries
    for (int32_t i = 0; i < count; i += 2)
    {
        bx_hmap_i32big_erase(&map, i);
    }
    assert(bx_hmap_i32big_size(&map) == (uint32_t)count / 2);

    for (int32_t i = 0; i < count; i++)
    {
        BigPayload* got = bx_hmap_i32big_get(&map, i);
        if (i % 2 == 0)
        {
            assert(got == NULL);
        }
        else
        {
            assert(got != NULL && payload_intact(got, i));
        }
    }

    bx_hmap_i32big_drop(&map);
}

void run_hmap_tests(void)
{
    printf("\n--- Starting hmap tests ---\n");
    test_hmap_large_entries();
    test_hmap_init_capacity();
    test_hmap_clear_reuses_buckets();
    test_hmap_pow2_logic();
    test_hmap_basic_ops();
    test_hmap_collisions_and_erasure();
    test_hmap_stress_resize();
    test_hmap_update_duplicate();
    test_hmap_wrap_around();
    test_hmap_minihash_collision();
    test_hmap_deep_backward_shift();
    test_hmap_empty_map_queries();
    test_hamp_complex_structures();
    printf("--- hmap tests passed ---\n\n");
}
