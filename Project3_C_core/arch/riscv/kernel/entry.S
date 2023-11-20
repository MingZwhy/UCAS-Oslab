#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  //step1: store sp into PCB_USER_SP
  sd sp, PCB_USER_SP(tp)

  //step2: load PCB_KERNEL_SP into sp
  //now sp point to bottom of kernel_stack
  ld sp, PCB_KERNEL_SP(tp)

  //step3: sp = sp + SWITCH_TO_SIZE
  //now sp point to regs_context_t
  addi sp, sp, SWITCH_TO_SIZE

  sd zero, OFFSET_REG_ZERO(sp)
  sd ra,   OFFSET_REG_RA(sp)
  //now sp is not origin sp, we will sd sp at last
  sd gp,   OFFSET_REG_GP(sp)
  //sd tp,   OFFSET_REG_TP(sp)    我们只在switch_to里换tp    

  sd t0,   OFFSET_REG_T0(sp)
  sd t1,   OFFSET_REG_T1(sp)
  sd t2,   OFFSET_REG_T2(sp)

  sd s0,   OFFSET_REG_S0(sp)
  sd s1,   OFFSET_REG_S1(sp)

  sd a0,   OFFSET_REG_A0(sp)
  sd a1,   OFFSET_REG_A1(sp)
  sd a2,   OFFSET_REG_A2(sp)
  sd a3,   OFFSET_REG_A3(sp)
  sd a4,   OFFSET_REG_A4(sp)
  sd a5,   OFFSET_REG_A5(sp)
  sd a6,   OFFSET_REG_A6(sp)
  sd a7,   OFFSET_REG_A7(sp)

  sd s2,   OFFSET_REG_S2(sp)
  sd s3,   OFFSET_REG_S3(sp)
  sd s4,   OFFSET_REG_S4(sp)
  sd s5,   OFFSET_REG_S5(sp)
  sd s6,   OFFSET_REG_S6(sp)
  sd s7,   OFFSET_REG_S7(sp)
  sd s8,   OFFSET_REG_S8(sp)
  sd s9,   OFFSET_REG_S9(sp)
  sd s10,  OFFSET_REG_S10(sp)
  sd s11,  OFFSET_REG_S11(sp)

  sd t3,   OFFSET_REG_T3(sp)
  sd t4,   OFFSET_REG_T4(sp)
  sd t5,   OFFSET_REG_T5(sp)
  sd t6,   OFFSET_REG_T6(sp)

  //ld sp from PCB_USER_SP(tp) and sd sp at last
  ld t0,   PCB_USER_SP(tp)
  sd t0,   OFFSET_REG_SP(sp)

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */

  /*
     privileged register 
  #define OFFSET_REG_SSTATUS      256
  #define OFFSET_REG_SEPC         264
  #define OFFSET_REG_SBADADDR     272
  #define OFFSET_REG_SCAUSE       280
  */

  li t0, SR_SUM | SR_FS
  //clear SR_SUM and SR_FS in reg STATUS
  csrc CSR_SSTATUS, t0

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */
  csrr t0, CSR_SSTATUS
  sd t0, OFFSET_REG_SSTATUS(sp)

  csrr t0, CSR_SEPC
  sd t0, OFFSET_REG_SEPC(sp)

  csrr t0, CSR_STVAL
  sd t0, OFFSET_REG_SBADADDR(sp)

  csrr t0, CSR_SCAUSE
  sd t0, OFFSET_REG_SCAUSE(sp)

  /*
  make sp point to pos of kernel_sp
  so that kernel_program can use space 
  between bottom of kernel_stack and switchto_context_t
  这里我们不要让sp指向我们写东西的部分,也就是kernel_sp以上
  可以让sp恰好指向kernel_sp，也就是让后序内核程序在RESTORE_CONTEXT
  之前用kernel_sp下面的一段空白空间，
  也可以直接ld sp, PCB_USER_SP(tp)让sp恢复进入SAVE_CONTEXT之前
  的值，这样子也是不会与kernel_sp以上内容冲突的
  */
  //addi sp, sp, -(SWITCH_TO_SIZE)
  ld sp,   PCB_USER_SP(tp)

.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
  ld sp, PCB_KERNEL_SP(tp) 
  addi sp, sp, SWITCH_TO_SIZE

  ld t0, OFFSET_REG_SEPC(sp)
  csrw CSR_SEPC, t0

  ld t0, OFFSET_REG_SSTATUS(sp)
  csrw CSR_SSTATUS, t0

  ld ra, OFFSET_REG_RA(sp)
  //we will restore sp at last 
  ld gp, OFFSET_REG_GP(sp)
  //ld tp, OFFSET_REG_TP(sp)

  ld t0, OFFSET_REG_T0(sp)
  ld t1, OFFSET_REG_T1(sp)
  ld t2, OFFSET_REG_T2(sp)

  ld s0, OFFSET_REG_S0(sp)
  ld s1, OFFSET_REG_S1(sp)

  ld a0, OFFSET_REG_A0(sp)
  ld a1, OFFSET_REG_A1(sp)
  ld a2, OFFSET_REG_A2(sp)
  ld a3, OFFSET_REG_A3(sp)
  ld a4, OFFSET_REG_A4(sp)
  ld a5, OFFSET_REG_A5(sp)
  ld a6, OFFSET_REG_A6(sp)
  ld a7, OFFSET_REG_A7(sp)

  ld s2, OFFSET_REG_S2(sp)
  ld s3, OFFSET_REG_S3(sp)
  ld s4, OFFSET_REG_S4(sp)
  ld s5, OFFSET_REG_S5(sp)
  ld s6, OFFSET_REG_S6(sp)
  ld s7, OFFSET_REG_S7(sp)
  ld s8, OFFSET_REG_S8(sp)
  ld s9, OFFSET_REG_S9(sp)
  ld s10,OFFSET_REG_S10(sp)
  ld s11,OFFSET_REG_S11(sp)

  ld t3, OFFSET_REG_T3(sp)
  ld t4, OFFSET_REG_T4(sp)
  ld t5, OFFSET_REG_T5(sp)
  ld t6, OFFSET_REG_T6(sp)

  //load sp in the end
  //注意这里ld的是存在regs_context里的user_sp
  ld sp, OFFSET_REG_SP(sp)

.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

//clear sip
ENTRY(clear_sip)
  //li t0, SR_SIE
  //csrc CSR_SIP, t0
  csrw CSR_SIP, zero
  jr ra
ENDPROC(clear_sip)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  //addi sp, sp, -(SWITCH_TO_SIZE)   (this is for another logic)

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  /*
  because we put kernel_sp in the first place of struct PCB,
  so the address of PCB is also the address of kernel_sp in struct PCB
  and that's also why now PCB_KERNEL_SP is 0
  */

  ld t0, PCB_KERNEL_SP(a0)

  /*
  *now we show the struct of kernel
  *sure now it is the stack of prev pcb

  *  / \  reg S11
  *   |     .
  *   |     .
  *   |     .
  *   |   reg S0
  *   |   reg SP
  *   |   reg RA    <-- (kernel_sp of previous pcb <==> t0) 
  */

  sd ra, SWITCH_TO_RA(t0)
  sd sp, SWITCH_TO_SP(t0)
  sd s0, SWITCH_TO_S0(t0)
  sd s1, SWITCH_TO_S1(t0)
  sd s2, SWITCH_TO_S2(t0)
  sd s3, SWITCH_TO_S3(t0)
  sd s4, SWITCH_TO_S4(t0)
  sd s5, SWITCH_TO_S5(t0)
  sd s6, SWITCH_TO_S6(t0)
  sd s7, SWITCH_TO_S7(t0)
  sd s8, SWITCH_TO_S8(t0)
  sd s9, SWITCH_TO_S9(t0)
  sd s10, SWITCH_TO_S10(t0)
  sd s11, SWITCH_TO_S11(t0)

  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  ld t0, PCB_KERNEL_SP(a1)

  /*
  *now we show the struct of kernel
  *sure now it is the stack of next pcb

  *  / \  reg S11
  *   |     .
  *   |     .
  *   |     .
  *   |   reg S0
  *   |   reg SP
  *   |   reg RA    <-- (kernel_sp of next pcb <==> t0) 
  */

  ld ra, SWITCH_TO_RA(t0)
  ld sp, SWITCH_TO_SP(t0)
  ld s0, SWITCH_TO_S0(t0)
  ld s1, SWITCH_TO_S1(t0)
  ld s2, SWITCH_TO_S2(t0)
  ld s3, SWITCH_TO_S3(t0)
  ld s4, SWITCH_TO_S4(t0)
  ld s5, SWITCH_TO_S5(t0)
  ld s6, SWITCH_TO_S6(t0)
  ld s7, SWITCH_TO_S7(t0)
  ld s8, SWITCH_TO_S8(t0)
  ld s9, SWITCH_TO_S9(t0)
  ld s10, SWITCH_TO_S10(t0)
  ld s11, SWITCH_TO_S11(t0)

  //make tp point to next_pcb
  addi tp, a1, 0

  //addi sp, sp, SWITCH_TO_SIZE  (this is for another logic)

  //jump to address in ra(in task1-2 is the entry_address of next task, in task3 is ret_from_exception)
  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  // for double core, release core
  call unlock_kernel
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT

  sret   
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)  

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT

  // for double core --> lock core
  //call lock_kernel

  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  ld t0, PCB_KERNEL_SP(tp)
  addi a0, t0, SWITCH_TO_SIZE    //regs_context_t *regs
  csrr a1, CSR_STVAL            
  csrr a2, CSR_SCAUSE

  call interrupt_helper

  la ra, ret_from_exception
  jr ra

ENDPROC(exception_handler_entry)