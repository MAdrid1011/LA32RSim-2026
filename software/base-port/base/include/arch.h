#ifndef ARCH_H__
#define ARCH_H__

#include <stdint.h>
typedef struct {
  uintptr_t gpr[32], mcause, mstatus ,mepc;
} Context ;

#define GPR1 gpr[17] // a7
#define GPR2 gpr[10]
#define GPR3 gpr[11]
#define GPR4 gpr[12]
#define GPRx gpr[10]

#define PAGE_SHIFT        12
#define PAGE_SIZE         (1ul << PAGE_SHIFT)

#endif