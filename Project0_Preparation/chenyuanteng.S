.text
.align 2
.globl main

main:
	li a0, 0
	li a1, 100
	li a2, 0
	
	Loop:
	addi a0, a0, 1
	add a2, a2, a0
	bne a0, a1, Loop
