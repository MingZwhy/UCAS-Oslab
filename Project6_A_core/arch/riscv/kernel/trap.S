#include <asm.h>
#include <csr.h>
ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0,exception_handler_entry
  csrw CSR_STVEC, t0
  addi a1, ra, 0
  /* TODO: [p2-task4] enable interrupts globally */
  //call enable_interrupt
  addi ra, a1, 0
  jr ra

ENDPROC(setup_exception)
