
.section entry, "ax"
.globl _start
.type _start, @function
#include <regdef.h>
_start:
  move s0, $r0
  // config the DMW for mmio
  li.w t0, 0xaa000009
  csrwr t0, csr_dmw0
  // config the DMW for common
  li.w t0, 0x00000019
  csrwr t0, csr_dmw1

  li.w t0, 0x10
  li.w t1, 0x1f
  csrxchg t0, t1, csr_crmd

  la $r3, _stack_pointer
  b call_main
  // b main

