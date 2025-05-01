
#include <common/assert.h>
#include <lib/def.h>
#include <lib/string.h>
#include <mm/vmm.h>

#define AT_NULL     0
#define AT_IGNORE   1
#define AT_EXECFD   2
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ   6
#define AT_BASE     7
#define AT_FLAGS    8
#define AT_ENTRY    9
#define AT_NOTELF  10
#define AT_UID     11
#define AT_EUID    12
#define AT_GID     13
#define AT_EGID    14

#define WRITE_QWORD(VALUE)                                               \
    {                                                                    \
        stack -= sizeof(u64);                                            \
        u64 tmp = (VALUE);                                               \
        size_t copyto_count = vmm_copy_to(as, stack, &tmp, 4);           \
        ASSERT(copyto_count == 4);                                       \
    }

#define WRITE_AUX(ID, VALUE) \
    {                        \
        WRITE_QWORD(VALUE);  \
        WRITE_QWORD(ID);     \
    }

uptr x86_64_abi_stack_setup(vmm_addr_space_t *as, size_t stack_size, char **argv, char **envp)
{
    ASSERT(stack_size % ARCH_PAGE_GRAN == 0);

    uptr stack_ptr = vmm_find_space(as, stack_size);
    vmm_map_anon(as, stack_ptr, stack_size, VMM_FULL);
    uptr stack = (uptr) stack_ptr + stack_size - 1;
    stack &= ~0xF;

    int argc = 0;
    for(; argv[argc]; argc++)
        stack -= strlen(argv[argc]) + 1;
    uptr arg_data = stack;

    int envc = 0;
    for(; envp[envc]; envc++)
        stack -= strlen(envp[envc]) + 1;
    uptr env_data = stack;

    stack -= (stack - (12 + 1 + envc + 1 + argc + 1) * sizeof(u64)) % 0x10;


    // Null auxiliary vector entry.
    WRITE_AUX(0, 0);
    // Auxiliary vector entries.
    // WRITE_AUX(AT_SECURE, 0);
    // WRITE_AUX(AT_ENTRY, auxv->entry);
    // WRITE_AUX(AT_PHDR, auxv->phdr);
    // WRITE_AUX(AT_PHENT, auxv->phent);
    // WRITE_AUX(AT_PHNUM, auxv->phnum);
    // 0
    WRITE_QWORD(0);
    // Environment pointers.
    for(int i = 0; i < envc; i++)
    {
        WRITE_QWORD(env_data);
        size_t str_sz = strlen(envp[i]) + 1;
        size_t copyto_count = vmm_copy_to(as, env_data, envp[i], str_sz);
        ASSERT(copyto_count == str_sz);
        env_data += str_sz;
    }
    // 0
    WRITE_QWORD(0);
    // Argument pointers.
    for(int i = 0; i < argc; i++)
    {
        WRITE_QWORD(arg_data);
        size_t str_sz = strlen(argv[i]) + 1;
        size_t copyto_count = vmm_copy_to(as, arg_data, argv[i], str_sz);
        ASSERT(copyto_count == str_sz);
        arg_data += str_sz;
    }
    // Argument count.
    WRITE_QWORD(argc);

    return stack;
}
