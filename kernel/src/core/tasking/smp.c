#include "tasking.h"

#include <arch/cpu.h>
#include <arch/init.h>

#include <core/mm/pmm.h>
#include <core/mm/kmem.h>
#include <core/tasking/tasking.h>
#include <core/tasking/sched.h>

#include <utils/assert.h>
#include <utils/log.h>
#include <utils/panic.h>
#include <utils/limine/requests.h>

list_t g_cpu_core_list = LIST_INIT;
bool g_smp_initialized = false;

static proc_t *g_idle_proc;

static void thread_idle_func()
{
    uint i = 0;
    while (true)
    {
        thread_t *idle_thread = arch_cpu_read_thread_reg();
        cpu_core_t *cpu_core = idle_thread->assigned_core;

        log("IDLE ON %u - %u", cpu_core->id, i++);

        sched_yield();
    }        
}

static void core_init(struct limine_mp_info *mp_info)
{
    static slock_t slock = SLOCK_INIT;
    slock_acquire(&slock); // Assure CPU cores are initialized sequentially.

    log("NEW CORE %u", mp_info->extra_argument);

    arch_cpu_core_init();

    thread_t *idle_thread = thread_new(g_idle_proc, (uptr)&thread_idle_func);
    cpu_core_t *cpu_core = kmem_alloc(sizeof(cpu_core_t));
    *cpu_core = (cpu_core_t) {
        .id = mp_info->extra_argument,
        .idle_thread = idle_thread
    };
    list_append(&g_cpu_core_list, &cpu_core->list_elem);

    idle_thread->assigned_core = cpu_core;
    arch_cpu_write_thread_reg(idle_thread);

    slock_release(&slock);

    thread_idle_func();
}

void smp_init()
{
    if (request_mp.response == NULL)
        panic("Invalid SMP info provided by the bootloader");

    g_idle_proc = proc_new(PROC_KERNEL);
    
    struct limine_mp_info *bsp_mp_info;
    for (size_t i = 0; i < request_mp.response->cpu_count; i++)
    {
        struct limine_mp_info *mp_info = request_mp.response->cpus[i];
        mp_info->extra_argument = i;

#if defined (__x86_64__)
        if (mp_info->lapic_id == request_mp.response->bsp_lapic_id)
#elif defined (__aarch64__)
        if (mp_info->mpidr == request_mp.response->bsp_mpidr)
#endif
        bsp_mp_info = mp_info;

        __atomic_store_n(&mp_info->goto_address, (limine_goto_address)&core_init, __ATOMIC_SEQ_CST);
    }

    // Also initialize the bootstrap processor.
    ASSERT(bsp_mp_info != NULL);
    core_init(bsp_mp_info);
}
