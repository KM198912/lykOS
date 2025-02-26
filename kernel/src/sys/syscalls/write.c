#include <arch/cpu.h>

#include <fs/vfs.h>
#include <sys/tasking.h>

#include <utils/log.h>

int write(int fd, u64 count, void *buf)
{
    log("WRITE");
    proc_t *proc = ((thread_t*)arch_cpu_read_thread_reg())->parent_proc;
    
    vfs_node_t *node = resource_get(&proc->resource_table, fd)->node;
    if (node == NULL)
        return -1;

    node->ops->write(node, 0, count, buf);
    return 0;
}
