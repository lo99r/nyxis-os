#ifndef KERNEL_H
#define KERNEL_H

#include "include/types.h"
#include "include/boot_info.h"

// Kernel main function - called from assembly entry point (_start)
void kernel_main(NTBLI* boot_info);

// Get kernel information (restricted access)
NTBLI* get_kernel_info();

#endif // KERNEL_H