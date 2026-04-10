
#ifndef ARGV
#define ARGV ""
#endif
#include <base.h>
#include <dev-mmio.h>
int main(const char *argv);
static const char argv[] = ARGV;

extern char _heap_start;
extern char _pmem_start;

#define PMEM_SIZE (128 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)&_pmem_start + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);

void putch(char ch) {
    outb(SERIAL_PORT, ch);
}
void halt(int code) {
  asm volatile("move $r4, %0; .word 0x80000000" : :"r"(code));
  while(1);
}

void call_main() {
  halt(main(argv));
}