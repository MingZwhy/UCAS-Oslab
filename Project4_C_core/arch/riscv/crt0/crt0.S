#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    addi sp, sp, -24
    sd a0, 0(sp)
    sd a1, 8(sp)

    la a0, __bss_start
    la a1, __BSS_END__

clbss:
    sw zero, 0(a0)
    addi a0, a0, 4
    blt a0, a1, clbss

    /* TODO: [p1-task3] enter main function */
    
    sd ra, 16(sp)
    ld a0, 0(sp)
    ld a1, 8(sp)
    call main

    /* TODO: [p1-task3] finish task and return to the kernel */
    /* NOTE: You need to replace this with new mechanism in p3-task2! */
    /* TODO: [P3] call sys_exit */
    ld ra, 16(sp)
    addi sp, sp, 24

    //jr ra
    call sys_exit

// while(1) loop, unreachable here
loop:
    //wfi
    j loop

END(_start)
