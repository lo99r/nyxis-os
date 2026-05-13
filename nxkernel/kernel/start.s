.section .text
.global _start
.type _start, @function

/*
 * Kernel Entry Point (_start)
 * 
 * Called by bootloader after ExitBootServices()
 * 
 * Arguments:
 *   edi (32-bit): pointer to NTBLI boot info structure
 */

_start:
    /* Set up stack */
    movq $0x1000000, %rsp        /* Stack pointer at high memory */
    sub $8, %esp                /* Align stack to 16 bytes for ABI compliance */

    /* Preserve boot info pointer */
    movq %rdi, %rax              /* Move boot_info pointer to rax (first argument) */
    pushq %rax                   /* Push as argument to kernel_main */

    /* Call kernel main */
    call kernel_main

    /* Halt if kernel returns (shouldn't happen) */
    cli
_kernel_returned:
    hlt
    jmp _kernel_returned

.size _start, . - _start
