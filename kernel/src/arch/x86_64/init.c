#include <arch/x86_64/devices/lapic.h>
#include <arch/x86_64/tables/gdt.h>
#include <arch/x86_64/tables/idt.h>
#include <arch/x86_64/tables/tss.h>
#include <arch/x86_64/fpu.h>
#include <arch/x86_64/syscall.h>

void arch_cpu_core_init()
{
    x86_64_gdt_load();
    x86_64_idt_load();
    x86_64_fpu_init_cpu();
    x86_64_lapic_init();
    x86_64_syscall_init();
}
