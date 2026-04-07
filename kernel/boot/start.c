#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

extern char timervec[];
extern void plicinithart(void);

extern void start_main(void);
uint64 timer_scratch[NCPU][5];

void plicinit(void)
{
  *(uint32 *)(PLIC + UART0_IRQ * 4) = 1;
  *(uint8 *)(UART0 + 1) = 1;
}

/* ================================================================
 * timerinit — 配置 CLINT 硬件定时器，使其定期产生 M-Mode 时钟中断
 *
 * 原理：
 *   CLINT 中的 mtime 寄存器一直在递增（硬件驱动）。
 *   当 mtime 超过 mtimecmp 时，触发 M-Mode 时钟中断。
 *   我们的 timervec 汇编入口：截获这个中断 → 把 mtimecmp 推迟 interval →
 *   向 S-Mode 注入一个软件中断（相当于通知 S-Mode 时钟到了）。
 * ================================================================ */
void timerinit(void)
{
  int hartid = r_mhartid();
  int interval = 1000000;
  *(uint64 *)CLINT_MTIMECMP(hartid) = *(uint64 *)CLINT_MTIME + interval;
  uint64 *scratch = &timer_scratch[hartid][0];
  scratch[3] = CLINT_MTIMECMP(hartid);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);
  w_mtvec((uint64)timervec);

  w_mie(r_mie() | MIE_MTIE);
  
}

/* ================================================================
 * start — M-Mode 主函数（在 entry.S 的栈设置完成后，立即被调用）
 *
 * 任务：
 *   1. 配置 mstatus：将"上一特权级"设为 S-Mode（MPP=01）
 *   2. 配置中断委托：把大多数中断/异常委托给 S-Mode 处理
 *   3. 初始化定时器
 *   4. 用 mret 指令降权跳入 S-Mode 的 start_main()
 * ================================================================ */
void start(void)
{
  uint64 x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK; 
  x |= MSTATUS_MPP_S;     
  w_mstatus(x);
  w_mepc((uint64)start_main);
  asm volatile("csrw pmpaddr0, %0" : : "r"(0x3fffffffffffffull));
  asm volatile("csrw pmpcfg0, %0" : : "r"(0xf));

  w_medeleg(0xffff); 
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
  timerinit();
  w_tp(r_mhartid());

  asm volatile("mret");
}
