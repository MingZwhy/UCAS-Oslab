#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50202000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

    // a0 is mhartid
	bnez a0, secondary

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li a0,BIOS_PUTSTR
	la a1,msg
	call bios_func_entry

	// TODO: [p1-task2] call BIOS to read kernel in SD card

	la t3,os_size_loc
    li a0,BIOS_SDREAD
    la a1,kernel
	lh a2,0(t3)
	li a3,0x1
    call bios_func_entry

	// TODO: [p1-task4] load task-related arguments and pass them to kernel
	li a0, BIOS_SDREAD
	li a1, 0x51a00000
	li x6, 0X502001f8
	li a2, 0x1
	lh a3, 0(x6)
	call bios_func_entry


	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	//j stop
	j kernel

secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */

  	la t0, kernel
  	csrw CSR_STVEC, t0
	li t1, SR_SIE
  	csrc CSR_SSTATUS, t1
  	li t0, 0x2
  	csrs CSR_SIE, t0
	csrs CSR_SSTATUS, t0

loop:
	j loop

// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader\n\r"

