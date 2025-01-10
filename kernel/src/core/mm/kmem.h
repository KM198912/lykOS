#pragma once

#include <utils/def.h>
#include <utils/list.h>
#include <utils/slock.h>

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
    slock_t lock;
    int assigned_cpu_id;
    list_node_t list_elem;
};

struct kmem_cache
{
    char name[32];
    uint obj_size;
    list_t slabs_partial;
    list_t slabs_full;
    slock_t slab_list_lock;
    list_node_t list_elem;
    kmem_slab_t *per_cpu_active_slab[];
};

kmem_cache_t *kmem_new_cache(char *name, uint obj_size);

void *kmem_alloc_from(kmem_cache_t *cache);

void* kmem_alloc(uint size);

void kmem_free(void *obj);

void kmem_init();

void kmem_debug();
