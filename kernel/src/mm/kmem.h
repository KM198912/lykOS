#pragma once

#include <common/sync/slock.h>
#include <lib/def.h>
#include <lib/list.h>

typedef struct kmem_slab kmem_slab_t;
typedef struct kmem_cache kmem_cache_t;

struct kmem_slab
{
    kmem_cache_t *parent_cache;
    uint obj_cnt;
    void *freelist;
    uint freelist_len;
    void *freelist_sec;
    uint freelist_sec_len;
    spinlock_t lock;
    int assigned_cpu_id;
    list_node_t list_elem;
};

struct kmem_cache
{
    char name[32];
    uint obj_size;
    list_t slabs_partial;
    list_t slabs_full;
    spinlock_t slab_list_lock;
    list_node_t list_elem;
    kmem_slab_t *per_cpu_active_slab[];
};

kmem_cache_t *kmem_new_cache(char *name, uint obj_size);

void *kmem_alloc_from(kmem_cache_t *cache);

void *kmem_alloc(uint size);

void kmem_free(void *obj);

void *kmem_realloc(void *obj, uint old_size, uint new_size);

void kmem_init();

void kmem_debug();

__attribute__((unused))
static void _cleanup_free(void *p)
{
    kmem_free(*(void**) p);
}

#define CLEANUP __attribute__((cleanup(_cleanup_free)))

#define CLEANUP_FUNC(func) __attribute__((cleanup(func)))
