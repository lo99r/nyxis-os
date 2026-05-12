#ifndef KERNEL_PAGING_H
#define KERNEL_PAGING_H

#include "include/nyxis.h"

// Page directory entry
typedef struct {
    u32 present    : 1;
    u32 rw         : 1;
    u32 user       : 1;
    u32 writethru  : 1;
    u32 cachedis   : 1;
    u32 accessed   : 1;
    u32 zero       : 1;
    u32 size       : 1;
    u32 ignored    : 4;
    u32 frame      : 20;
} page_directory_entry_t;

// Page table entry
typedef struct {
    u32 present    : 1;
    u32 rw         : 1;
    u32 user       : 1;
    u32 writethru  : 1;
    u32 cachedis   : 1;
    u32 accessed   : 1;
    u32 dirty      : 1;
    u32 zero       : 1;
    u32 global     : 1;
    u32 ignored    : 3;
    u32 frame      : 20;
} page_table_entry_t;

// Page directory
typedef struct {
    page_directory_entry_t entries[1024];
} page_directory_t;

// Page table
typedef struct {
    page_table_entry_t entries[1024];
} page_table_t;

// Functions
Nstatus paging_init();
Nstatus paging_map_page(void* phys, void* virt, u32 flags);
Nstatus paging_unmap_page(void* virt);
void paging_enable();
void paging_disable();

#endif // KERNEL_PAGING_H