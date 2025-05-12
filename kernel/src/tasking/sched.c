#include "sched.h"

#include <arch/cpu.h>
#include <arch/x86_64/fpu.h>
#include <arch/x86_64/msr.h>
#include <common/log.h>
#include <common/panic.h>
#include <common/sync/spinlock.h>
#include <sys/smp.h>
#include <sys/thread.h>

extern __attribute__((naked)) void arch_sched_context_switch(thread_t *curr, thread_t *next);

static spinlock_t slock = SPINLOCK_INIT;
static list_t g_thread_list = LIST_INIT;

static thread_t *sched_next()
{
    thread_t *ret = NULL;

    spinlock_acquire(&slock);

    list_node_t *node = list_pop_head(&g_thread_list);
    if (node != NULL)
        ret = LIST_GET_CONTAINER(node, thread_t, list_elem_thread);

    spinlock_release(&slock);

    return ret;
}

thread_t *sched_get_curr_thread()
{
    return (thread_t *)arch_cpu_read_thread_reg();
}

void sched_drop(thread_t *thread)
{
    if (thread != sched_get_curr_thread()->assigned_core->idle_thread
    &&  thread->status == THREAD_STATE_READY)
        sched_queue_add(thread);
}

void sched_queue_add(thread_t *thread)
{
    spinlock_acquire(&slock);

    list_append(&g_thread_list, &thread->list_elem_thread);

    spinlock_release(&slock);
}

void sched_yield(thread_status_t status)
{
    thread_t *curr = arch_cpu_read_thread_reg();
    thread_t *next = sched_next();

    if (next == NULL)
        next = sched_get_curr_thread()->assigned_core->idle_thread;
    if (curr != next)
    {
        next->assigned_core = curr->assigned_core;
        curr->assigned_core = NULL;
        curr->status = status;
        next->status = THREAD_STATE_RUNNING;

        vmm_load_addr_space(next->parent_proc->addr_space);
    }
    g_x86_64_fpu_save(curr->fpu_area);
    g_x86_64_fpu_restore(next->fpu_area);
    curr->gs = x86_64_msr_read(X86_64_MSR_KERNEL_GS_BASE);
    curr->fs = x86_64_msr_read(X86_64_MSR_FS_BASE);
    x86_64_msr_write(X86_64_MSR_KERNEL_GS_BASE, next->gs);
    x86_64_msr_write(X86_64_MSR_FS_BASE, next->fs);
    arch_cpu_write_thread_reg(next);
    arch_sched_context_switch(curr, next); // This function calls `sched_drop` for `curr` too.
}
