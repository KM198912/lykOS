#pragma once

#include <fs/vfs.h>
#include <lib/list.h>

typedef struct
{
    void (*install)();
    void (*destroy)();
    void (*probe)();

    vfs_node_t *mod_fs_node;
}
module_t;

extern list_t g_mod_module_list;

void mod_init();

module_t *module_load(vfs_node_t *file);