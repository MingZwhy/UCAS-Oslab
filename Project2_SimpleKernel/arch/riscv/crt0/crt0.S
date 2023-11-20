#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la a0, __bss_start
    la a1, __BSS_END__

clbss:
    sw zero, 0(a0)
    addi a0, a0, 4
    blt a0, a1, clbss

    /* TODO: [p1-task3] enter main function */
    addi sp, sp, -16
    sd ra, 8(sp)
    call main

    /* TODO: [p1-task3] finish task and return to the kernel */
    /* NOTE: You need to replace this with new mechanism in p3-task2! */
    ld ra, 8(sp)
    addi sp, sp, 16

    jr ra

// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
