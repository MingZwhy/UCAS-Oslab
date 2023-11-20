/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  //p14:链接器脚本中定义的符号可以直接引用
  //汇编中直接用la ; C语言中先声明extern在&获得其地址
  //riscv.lds  -->  __bss_start与__BSS_END__
  la x5, __bss_start
  la x6, __BSS_END__
  addi x7, x0, 0
  
clbss:
  sw x7, 0(x5)
  addi x5, x5, 4
  bne x5, x6, clbss
 
  /* TODO: [p1-task2] setup C environment */
  la sp, KERNEL_STACK
  call main


loop:
  wfi
  j loop

END(_start)


