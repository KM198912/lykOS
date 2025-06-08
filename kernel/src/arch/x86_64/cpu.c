#include <arch/cpu.h>
#include <arch/x86_64/msr.h>
#include <arch/x86_64/tables/gdt.h>
#include <arch/x86_64/tables/tss.h>
#include <mm/heap.h>

void arch_cpu_context_init(arch_cpu_context_t *context)
{
    context->tss = heap_alloc(sizeof(x86_64_tss_t));
    x86_64_gdt_load_tss(context->tss);
}

void *arch_cpu_read_thread_reg()
{
    void *thread = NULL;
    asm volatile("mov %%gs:0, %0" : "=r"(thread));

    return thread;
}

void arch_cpu_write_thread_reg(void *t)
{
    x86_64_msr_write(X86_64_MSR_GS_BASE, (u64)t);
}

void arch_cpu_halt()
{
    while (true)
        asm volatile("hlt");
}

void arch_cpu_relax()
{
    asm volatile("pause");
}

void arch_cpu_int_unmask()
{
    asm volatile("sti");
}

void arch_cpu_int_mask()
{
    asm volatile("cli");
}

bool arch_cpu_int_enabled()
{
    u64 flags;
    asm volatile ("pushfq; popq %0" : "=r"(flags));
    return (flags & (1 << 9)) != 0;
}
