#pragma once

#include <arch/ptm.h>
#include <arch/types.h>

#include <common/sync/spinlock.h>
#include <lib/def.h>
#include <lib/list.h>

// Types

typedef struct
{
    spinlock_t slock;
    list_t segments;
    uptr limit_low;
    uptr limit_high;

    arch_ptm_map_t ptm_map;
}
vmm_addr_space_t;

typedef enum
{
    VMM_SEG_ANON,
    VMM_SEG_KERNEL
}
vmm_seg_type_t;

typedef enum
{
    VMM_NONE  = 0,
    VMM_READ  = 1 << 0,
    VMM_WRITE = 1 << 1,
    VMM_EXEC  = 1 << 2,
    VMM_FULL  = VMM_READ | VMM_WRITE | VMM_EXEC
}
vmm_prot_t;

typedef struct
{
    vmm_addr_space_t *addr_space;
    vmm_seg_type_t type;
    uptr base;
    u64  len;

    list_node_t list_elem;
}
vmm_seg_t;

// Public

extern vmm_addr_space_t *g_vmm_kernel_addr_space;

uptr vmm_find_space(vmm_addr_space_t *addr_space, u64 len);

bool vmm_pagefault_handler(vmm_addr_space_t *addr_space, uptr addr);

void vmm_map_anon(vmm_addr_space_t *addr_space, uptr virt, u64 len, vmm_prot_t prot);

void vmm_map_kernel(vmm_addr_space_t *addr_space, uptr virt, u64 len, vmm_prot_t prot, uptr phys);

uptr vmm_virt_to_phys(vmm_addr_space_t *addr_space, uptr virt);

vmm_addr_space_t *vmm_new_addr_space(uptr limit_low, uptr limit_high);

void vmm_load_addr_space(vmm_addr_space_t *addr_space);

u64 vmm_copy_to(vmm_addr_space_t *dest_as, uptr dest_addr, void *src, u64 count);

u64 vmm_zero_out(vmm_addr_space_t *dest_as, uptr dest_addr, u64 count);

void vmm_init();
