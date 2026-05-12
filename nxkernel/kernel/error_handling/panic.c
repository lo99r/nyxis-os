#pragma once

#include "include/nyxis.h"
#include "include/lowlevel.h"

__attribute__((noreturn))
static inline void kernel_panic_simple(const char* Message, Nstatus error) {
    printk("\n");
    printk("========================================\n");
    printk("            KERNEL PANIC\n");
    printk("========================================\n");

    if (Message) {
        printk("Reason: %s%r\n", Message, error);
    }

    printk("System halted.\n");

    for (;;) {
        cli();
        hlt();
    }
}
