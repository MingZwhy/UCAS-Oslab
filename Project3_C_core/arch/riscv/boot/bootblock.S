#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	//a0 is mhartid
	bnez a0, secondary

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	//void bios_putstr(char *str)
	la a1, msg	          //step1: load address of msg into $a1
	li a0, BIOS_PUTSTR	  //step2: load BIOS_PUTSTR into $a0
	call bios_func_entry  //step3: call func

	// TODO: [p1-task2] call BIOS to read kernel in SD card
	//uintptr_t bios_sdread(unsigned mem_address, unsigned num_of_blocks, unsigned block_id)
	li a0, BIOS_SDREAD
	li a1, kernel           //mem_address
	li x6, 0x502001fc       //0x502001fc --> record the number of sectors kernel holds
	lh a2, 0(x6)		//load the number of sectors into a2
	li a3, 1		        //block_id --> kernel --> in SD[1] 
	call bios_func_entry    //call func

	// TODO: [p1-task4] load task-related arguments and pass them to kernel
	li a0, BIOS_SDREAD
	li a1, 0x52030000
	li x6, 0X502001f4
	lh a2, 4(x6)
	lh a3, 2(x6)
	call bios_func_entry


	lh a0, 6(x6)
	lh a1, 0(x6)

	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j  kernel   //跳转至kernel
	//j stop


secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */

	//step1:mask all interrupts
  	li t0, SR_SIE
  	csrc CSR_SSTATUS, t0

	//step2:let stvec pointer to kernel_main
  	la t0, kernel
  	csrw CSR_STVEC, t0

	//step3:enable software interrupt for ipi
  	li t0, 0x00000002
  	csrs CSR_SIE, t0
	csrs CSR_SSTATUS, t0


wait_for_wakeup:
	//wfi
	j wait_for_wakeup


// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

