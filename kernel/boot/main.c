#include "riscv.h"

extern void uart_puts(char *s);
extern void printf(char *fmt, ...);
extern void clear_screen();
extern void kinit();
extern void kvmininit();
extern void kvminithart();
extern void start();
extern void trapinithart();
extern void plicinithart();
extern void intr_on();
extern void scheduler();
extern void procinit();
extern void userinit();

void start_main()
{

  kinit(), kvmininit(), kvminithart();
  trapinithart();
  intr_on();
  procinit();
  userinit();
  userinit();
  userinit();
  userinit();
  scheduler();
}
