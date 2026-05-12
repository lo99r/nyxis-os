#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "include/nyxis.h"

// Process states
typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

// Process structure
typedef struct process {
    u32 pid;
    process_state_t state;
    void* stack;
    void* entry_point;
    struct process* next;
    // Registers for context switching
    u32 eax, ebx, ecx, edx;
    u32 esi, edi, ebp, esp;
    u32 eip, eflags;
    u32 cr3; // Page directory
} process_t;

// Functions
Nstatus process_init(void*);
Nstatus process_create(void* entry_point, void* stack);
Nstatus process_switch(process_t* proc);
Nstatus process_terminate(u32 pid);

#endif // KERNEL_PROCESS_H
