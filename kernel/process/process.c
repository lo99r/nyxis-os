#include "kernel/process/process.h"
#include "kernel/paging/paging.h"

// Process list
static process_t* process_list = 0;
static u32 next_pid = 1;

// Current process
static process_t* current_process = 0;

// Initialize process system
Nstatus process_init() {
    // Create idle process
    process_t* idle = (process_t*)0x200000; // Example allocation
    idle->pid = 0;
    idle->state = PROCESS_RUNNING;
    idle->stack = (void*)0x300000;
    idle->entry_point = 0;
    idle->next = 0;

    process_list = idle;
    current_process = idle;

    return NSTATUS_OK;
}

// Create a new process
Nstatus process_create(void* entry_point, void* stack) {
    process_t* proc = (process_t*)0x201000; // Example allocation
    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->stack = stack;
    proc->entry_point = entry_point;
    proc->next = process_list;
    process_list = proc;

    // Initialize registers
    proc->esp = (u32)stack;
    proc->eip = (u32)entry_point;
    proc->eflags = 0x202; // IF flag set

    return NSTATUS_OK;
}

// Switch to a process
Nstatus process_switch(process_t* proc) {
    if (!proc || proc->state != PROCESS_READY) {
        return NSTATUS_ERR_FLAG | 1; // Error
    }

    // Save current context
    if (current_process) {
        __asm__ volatile (
            "mov %%eax, %0\n"
            "mov %%ebx, %1\n"
            "mov %%ecx, %2\n"
            "mov %%edx, %3\n"
            "mov %%esi, %4\n"
            "mov %%edi, %5\n"
            "mov %%ebp, %6\n"
            "mov %%esp, %7\n"
            : "=m"(current_process->eax), "=m"(current_process->ebx),
              "=m"(current_process->ecx), "=m"(current_process->edx),
              "=m"(current_process->esi), "=m"(current_process->edi),
              "=m"(current_process->ebp), "=m"(current_process->esp)
        );
    }

    // Switch page directory
    if (proc->cr3) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(proc->cr3));
    }

    // Load new context
    current_process = proc;
    proc->state = PROCESS_RUNNING;

    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov %1, %%ebx\n"
        "mov %2, %%ecx\n"
        "mov %3, %%edx\n"
        "mov %4, %%esi\n"
        "mov %5, %%edi\n"
        "mov %6, %%ebp\n"
        "mov %7, %%esp\n"
        "jmp *%8\n"
        :
        : "m"(proc->eax), "m"(proc->ebx), "m"(proc->ecx), "m"(proc->edx),
          "m"(proc->esi), "m"(proc->edi), "m"(proc->ebp), "m"(proc->esp),
          "m"(proc->eip)
    );

    return NSTATUS_OK;
}

// Terminate a process
Nstatus process_terminate(u32 pid) {
    process_t* proc = process_list;
    while (proc) {
        if (proc->pid == pid) {
            proc->state = PROCESS_TERMINATED;
            // Remove from list, free memory, etc.
            return NSTATUS_OK;
        }
        proc = proc->next;
    }
    return NSTATUS_ERR_FLAG | 2; // Not found
}