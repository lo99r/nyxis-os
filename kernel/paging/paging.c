#include "kernel/paging/paging.h"
#include "include/lowlevel.h"

// Current page directory
static page_directory_t* current_page_directory;

// Initialize paging
Nstatus paging_init() {
    // Allocate page directory
    current_page_directory = (page_directory_t*)0x100000; // Example address, should be allocated properly

    // Clear page directory
    memset(current_page_directory, 0, sizeof(page_directory_t));

    // Identity map first 4MB
    for (u32 i = 0; i < 1024; i++) {
        page_table_t* table = (page_table_t*)0x101000; // Example
        memset(table, 0, sizeof(page_table_t));

        current_page_directory->entries[i].present = 1;
        current_page_directory->entries[i].rw = 1;
        current_page_directory->entries[i].frame = (u32)table >> 12;

        for (u32 j = 0; j < 1024; j++) {
            table->entries[j].present = 1;
            table->entries[j].rw = 1;
            table->entries[j].frame = (i * 1024 + j);
        }
    }

    return NSTATUS_OK;
}

// Map a physical page to virtual address
Nstatus paging_map_page(void* phys, void* virt, u32 flags) {
    u32 virt_addr = (u32)virt;
    u32 phys_addr = (u32)phys;

    // Get page directory index
    u32 pd_index = virt_addr >> 22;
    u32 pt_index = (virt_addr >> 12) & 0x3FF;

    // Get page table
    page_table_t* table = (page_table_t*)(current_page_directory->entries[pd_index].frame << 12);

    // Map the page
    table->entries[pt_index].present = 1;
    table->entries[pt_index].rw = (flags & 0x1) ? 1 : 0;
    table->entries[pt_index].user = (flags & 0x2) ? 1 : 0;
    table->entries[pt_index].frame = phys_addr >> 12;

    // Invalidate TLB
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");

    return NSTATUS_OK;
}

// Unmap a virtual page
Nstatus paging_unmap_page(void* virt) {
    u32 virt_addr = (u32)virt;

    u32 pd_index = virt_addr >> 22;
    u32 pt_index = (virt_addr >> 12) & 0x3FF;

    page_table_t* table = (page_table_t*)(current_page_directory->entries[pd_index].frame << 12);

    table->entries[pt_index].present = 0;

    // Invalidate TLB
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");

    return NSTATUS_OK;
}

// Enable paging
void paging_enable() {
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(current_page_directory)
        : "eax", "memory"
    );
}

// Disable paging
void paging_disable() {
    __asm__ volatile (
        "mov %%cr0, %%eax\n"
        "and $0x7FFFFFFF, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        :
        : "eax"
    );
}