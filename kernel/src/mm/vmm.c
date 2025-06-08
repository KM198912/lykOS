#include "vmm.h"
#include "arch/types.h"

#include <arch/ptm.h>
#include <common/assert.h>
#include <common/hhdm.h>
#include <common/limine/requests.h>
#include <common/log.h>
#include <common/sync/spinlock.h>
#include <lib/list.h>
#include <lib/math.h>
#include <lib/string.h>
#include <mm/kmem.h>
#include <mm/heap.h>
#include <mm/pmm.h>

#define SEG_INTERSECTS(BASE1, LENGTH1, BASE2, LENGTH2) ((BASE1) < ((BASE2) + (LENGTH2)) && (BASE2) < ((BASE1) + (LENGTH1)))

vmm_addr_space_t *g_vmm_kernel_addr_space;

static kmem_cache_t *g_segment_cache;

static void vmm_insert_seg(vmm_addr_space_t *addr_space, vmm_seg_t *seg)
{
    spinlock_acquire(&addr_space->slock);

    list_node_t *pos = NULL;
    FOREACH(n, addr_space->segments)
    {
        vmm_seg_t *i = LIST_GET_CONTAINER(n, vmm_seg_t, list_elem);

        if (i->base < seg->base)
            pos = n;
        else
            break; // Given that the list is sorted, an earlier position must have been found.
    }

    if (pos)
        list_insert_after(&addr_space->segments, pos, &seg->list_elem);
    else
        list_prepend(&addr_space->segments, &seg->list_elem);

    spinlock_release(&addr_space->slock);
}

uptr vmm_find_space(vmm_addr_space_t *addr_space, u64 len)
{
    if (list_is_empty(&addr_space->segments))
        return addr_space->limit_low;

    uptr start = addr_space->limit_low;
    FOREACH(n, addr_space->segments)
    {
        vmm_seg_t *seg = LIST_GET_CONTAINER(n, vmm_seg_t, list_elem);
        // If there's enough space between current start and this segment.
        if (start + len < seg->base)
            break;
        // Update start to point to the end of this segment.
        start = seg->base + seg->len;
    }

    // Check if there us space after the last segment.
    if (start + len - 1 <= addr_space->limit_high)
        return start;

    return 0;
}

vmm_seg_t *vmm_addr_to_seg(vmm_addr_space_t *addr_space, uptr addr)
{
    FOREACH (n, addr_space->segments)
    {
        vmm_seg_t *seg = LIST_GET_CONTAINER(n, vmm_seg_t, list_elem);

        if (seg->base <= addr && addr < seg->base + seg->len)
            return seg;
    }

    return NULL;
}

bool vmm_pagefault_handler(vmm_addr_space_t *addr_space, uptr addr)
{
    vmm_seg_t *seg = vmm_addr_to_seg(addr_space, addr);
    if (seg == NULL)
    {
        log("VMM: bruh");
        return false;
    }

    switch (seg->type)
    {
        case VMM_SEG_ANON:
        {
            uptr virt = FLOOR(addr, ARCH_PAGE_GRAN);
            uptr phys = (uptr)pmm_alloc(0);
            arch_ptm_map(
                &seg->addr_space->ptm_map,
                virt,
                phys,
                ARCH_PAGE_GRAN
            );
            return true;
        }
        default:
            return false;
    }
}

void vmm_map_anon(vmm_addr_space_t *addr_space, uptr virt, u64 len, vmm_prot_t prot)
{
    (void)(prot);
    ASSERT(virt % ARCH_PAGE_GRAN == 0 && len % ARCH_PAGE_GRAN == 0);

    vmm_seg_t *created_seg = kmem_alloc_cache(g_segment_cache);
    *created_seg = (vmm_seg_t) {
        .addr_space = addr_space,
        .type = VMM_SEG_ANON,
        .base = virt,
        .len  = len,
        .list_elem = LIST_NODE_INIT
    };
    vmm_insert_seg(addr_space, created_seg);
}

void vmm_map_kernel(vmm_addr_space_t *addr_space, uptr virt, u64 len, vmm_prot_t prot, uptr phys)
{
    (void)(prot);
    ASSERT(virt % ARCH_PAGE_GRAN == 0 && len % ARCH_PAGE_GRAN == 0);

    vmm_seg_t *created_seg = kmem_alloc_cache(g_segment_cache);
    *created_seg = (vmm_seg_t) {
        .addr_space = addr_space,
        .type = VMM_SEG_KERNEL,
        .base = virt,
        .len  = len,
        .list_elem = LIST_NODE_INIT
    };
    vmm_insert_seg(addr_space, created_seg);

    spinlock_acquire(&addr_space->slock);

    for (uptr addr = 0; addr < len; addr += ARCH_PAGE_SIZE_4K)
        arch_ptm_map(&addr_space->ptm_map, virt + addr, phys + addr, ARCH_PAGE_SIZE_4K);

    spinlock_release(&addr_space->slock);
}

uptr vmm_virt_to_phys(vmm_addr_space_t *addr_space, uptr virt)
{
    return arch_ptm_virt_to_phys(&addr_space->ptm_map, virt);
}

vmm_addr_space_t *vmm_new_addr_space(uptr limit_low, uptr limit_high)
{
    vmm_addr_space_t *addr_space = heap_alloc(sizeof(vmm_addr_space_t));

    *addr_space = (vmm_addr_space_t) {
        .slock = SPINLOCK_INIT,
        .segments = LIST_INIT,
        .limit_low = limit_low,
        .limit_high = limit_high,
        .ptm_map = arch_ptm_new_map()
    };
    return addr_space;
}

void vmm_load_addr_space(vmm_addr_space_t *addr_space)
{
    arch_ptm_load_map(&addr_space->ptm_map);
}

void vmm_init()
{
    if (request_memmap.response == NULL)
        panic("Invalid memory map provided by bootloader!");

    arch_ptm_init();

    g_segment_cache = kmem_new_cache("VMM Segment Cache", sizeof(vmm_seg_t));

    g_vmm_kernel_addr_space = vmm_new_addr_space(ARCH_KERNEL_MIN_VIRT, ARCH_KERNEL_MAX_VIRT);

    // Mappings done according to the Limine specification.
    vmm_map_kernel(
        g_vmm_kernel_addr_space,
        HHDM,
        4 * GIB,
        VMM_FULL,
        0
    );
    vmm_map_kernel(
        g_vmm_kernel_addr_space,
        request_kernel_addr.response->virtual_base,
        2 * GIB,
        VMM_FULL,
        request_kernel_addr.response->physical_base
    );
    for (u64 i = 0; i < request_memmap.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = request_memmap.response->entries[i];
        if (e->type == LIMINE_MEMMAP_RESERVED ||
            e->type == LIMINE_MEMMAP_BAD_MEMORY)
            continue;

        uptr base   = FLOOR(e->base, ARCH_PAGE_GRAN);
        u64  length = CEIL(e->base + e->length, ARCH_PAGE_GRAN) - base;

        vmm_map_kernel(
            g_vmm_kernel_addr_space,
            base + HHDM,
            length,
            VMM_FULL,
            base
        );
    }

    vmm_load_addr_space(g_vmm_kernel_addr_space);

    log("VMM initialized.");
}

u64 vmm_copy_to(vmm_addr_space_t *dest_as, uptr dest_addr, void *src, u64 count)
{
    u64 i = 0;
    while (i < count)
    {
        u64 offset = (dest_addr + i) % ARCH_PAGE_GRAN;
        uptr phys = vmm_virt_to_phys(dest_as, dest_addr + i);
        if (phys == BAD_ADDRESS)
        {
            vmm_pagefault_handler(dest_as, dest_addr + i);
            phys = vmm_virt_to_phys(dest_as, dest_addr + i);
            if (phys == BAD_ADDRESS)
                panic("tf");
        }

        u64 len = MIN(count - i, ARCH_PAGE_GRAN - offset);
        memcpy((void*)(phys + HHDM), src, len);
        i += len;
        src += len;
    }
    return i;
}

u64 vmm_zero_out(vmm_addr_space_t *dest_as, uptr dest_addr, u64 count)
{
    u64 i = 0;
    while (i < count)
    {
        u64 offset = (dest_addr + i) % ARCH_PAGE_GRAN;
        uptr phys = vmm_virt_to_phys(dest_as, dest_addr + i);
        if (phys == BAD_ADDRESS)
        {
            vmm_pagefault_handler(dest_as, dest_addr + i);
            phys = vmm_virt_to_phys(dest_as, dest_addr + i);
            if (phys == BAD_ADDRESS)
                panic("tf");
        }

        u64 len = MIN(count - i, ARCH_PAGE_GRAN - offset);
        memset((void*)(phys + HHDM), 0, len);
        i += len;
    }
    return i;
}
