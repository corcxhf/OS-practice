#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

/* 时钟中断处理代码的入口（汇编实现），在 M-Mode 触发时调用 */
extern char timervec[];
extern void plicinithart(void);

extern void start_main(void);
/* 每个 CPU 核心的临时存储区，timervec 汇编代码需要用到 */
uint64 timer_scratch[NCPU][5];

void plicinit(void)
{
  // 1. 设置 UART0 的中断优先级为 1（默认是 0，表示关闭）
  // PLIC_PRIORITY 寄存器数组，每个设备占 4 个字节
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
  /* 获取当前 CPU 核心编号：在 entry.S 中被存入了 tp 寄存器 */
  int hartid = r_mhartid();

  /* 设置每次时钟中断的间隔：约 0.1 秒（具体时间取决于 QEMU 的时钟频率）*/
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
  printf("START!\n");
  /* 将 "上一特权级" 设为 S-Mode，这样执行 mret 时会进入 S-Mode */
  uint64 x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK; /* 清除 MPP 字段 */
  x |= MSTATUS_MPP_S;     /* 设置 MPP = 01（S-Mode）*/
  w_mstatus(x);

  /* 设置 mepc 为 start_main 的地址，mret 执行后 PC 会跳到这里 */
  w_mepc((uint64)start_main);

  asm volatile("csrw pmpaddr0, %0" : : "r"(0x3fffffffffffffull));
  asm volatile("csrw pmpcfg0, %0" : : "r"(0xf));

  /* 将所有中断和异常委托给 S-Mode 处理（不需要 M-Mode 转手）*/
  w_medeleg(0xffff); /* 委托所有同步异常 */
  w_mideleg(0xffff); /* 委托所有中断 */

  /* 在 S-Mode 中开启时钟中断和软件中断的使能位 */
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  /* 初始化 M-Mode 定时器 */
  timerinit();

  /* 将当前 CPU 核心编号（hartid）保存在 tp 寄存器中 */
  /* （mycpu() 函数会通过读取 tp 来知道当前是哪个核心）*/
  w_tp(r_mhartid());

  /* 执行 mret：降权到 S-Mode，跳转到 mepc 所指的 start_main() */
  asm volatile("mret");

  /* 不会到达这里 */
}
