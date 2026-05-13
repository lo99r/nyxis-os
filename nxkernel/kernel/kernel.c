#include "kernel/kernel.h"
// #include "kernel/process/process.h"
#include "types.h"
#include "boot_info.h"
#include "lowlevel.h"
#include "console/outputs/printk.h"

// Kernel information (NTBLI) - stored in kernel space
// Only accessible via get_kernel_info() function
static NTBLI* kernel_info = nNULL;

// Get kernel information - this is the only way to access NTBLI
// Only the kernel process or privileged code should call this
NTBLI* get_kernel_info() {
    // In a full implementation, add privilege checks here
    // For now, return the kernel info
    return kernel_info;
}

// Kernel main function - called from assembly entry point
void kernel_main(NTBLI* boot_info) {
    // Store boot information in kernel space
    kernel_info = boot_info;

    // Initialize kernel subsystems
    Nstatus status;

    // Initialize process management
    // status = process_init();
    // if (NSTATUS_IS_ERR(status)) {
    //     hlt();
    // }

    // Initialize console output
    status = printk_init(
        boot_info->framebuffer_base,
        boot_info->pixels_per_scan_line,
        boot_info->width,
        boot_info->height
    );
    if (NSTATUS_IS_ERR(status)) {
        hlt();
    }

    // Print boot message
    printk("Nyxis OS Kernel Started\n");
    printk("Resolution: %ux%u\n", boot_info->width, boot_info->height);

    // TODO: Create first user process
    // TODO: Set up interrupt handlers
    // TODO: Initialize other subsystems

    // For now, just loop
    while (1) {
        hlt();
    }
}