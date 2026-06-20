/* trap.c — 中断与异常分发（Lab4 任务1&3，Lab6 扩展）
 *
 * 本文件是内核的"中控室"。当 sys_trap_vector 把寄存器保存完毕，
 * 就会调用 sys_trap_handler()，由它来判断发生了什么事并分派处理。
 *
 * Lab4 实现：处理时冲中断，每次打印 "Tick!"
 * Lab5 扩展：在时钟中断中增加 yield()，触发进程调度
 * Lab6 扩展：增加 usertrap()，处理来自用户态的 ecall 系统调用
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "proc.h"
#include "types.h"

/* 声明 sys_trap_vector 汇编入口（在 kernelvec.S 中定义）*/
extern char sys_trap_vector[];
extern pagetable_t kernel_pagetable;
extern void plicinit(void);
extern void plicinithart(void);
extern void uservec();
extern void consoleintr(int);
extern void userret(uint64, uint64);
extern void virtio_disk_intr();

static void usertrap_finish(void)
{
  struct proc *p = myproc();
  if (p && p->killed)
    proc_exit(-1);
  usertrapret();
}
/* ================================================================
 * trapinithart — 设置 S-Mode 陷阱向量
 *
 * 告诉 CPU：当 S-Mode 下发生中断/异常时，跳转到 sys_trap_vector。
 * 在 main.c 的 start_main() 中调用一次即可（每个 CPU 核心调用一次）。
 * ================================================================ */
void trapinithart(void)
{
  w_stvec((uint64)sys_trap_vector);
  plicinit();
  plicinithart();
  // w_sstatus(r_sstatus() | SSTATUS_SIE);
}

/* ================================================================
 * sys_trap_handler — 内核态中断/异常总处理函数（由 sys_trap_vector 汇编调用）
 *
 * 本函数从 CSR 读取中断原因，然后根据类型分发处理：
 *
 *   scause 最高位（bit 63）：
 *     = 1 → 异步中断（Interrupt），低位表示具体类型
 *     = 0 → 同步异常（Exception），不应在内核中发生
 *
 *   常见中断类型（irq 值）：
 *     1  → 软件中断（由 M-Mode 的 timervec 注入的时钟信号）
 *     5  → S-Mode 时钟中断（如果直接委托到 S-Mode）
 *     9  → 外部中断（UART 键盘输入等）
 * ================================================================ */

void sys_trap_handler(void)
{
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  /* 验证：进入内核陷阱前，S-Mode 的中断应该已经关闭 */
  if ((sstatus & SSTATUS_SPP) == 0)
    panic("sys_trap_handler: not from supervisor mode");

  if (intr_get())
    panic("sys_trap_handler: entered with interrupts enabled");

  if (scause & 0x8000000000000000L)
  {
    /* 这是一个异步中断 */
    uint64 irq = scause & 0xff;

    switch (irq)
    {
    case 1:
      w_sip(r_sip() & ~2);
      // printf("KERNEL TICK!\n");
      if (myproc() != 0 && myproc()->status == TASK_RUNNING)
        yield();
      break;

    case 9:
    {

      int hartid = r_tp();
      int irq = *(uint32 *)PLIC_SCLAIM(hartid);

      if (irq == UART0_IRQ)
        while ((*(uint8 *)(UART0 + 5) & 1) != 0)
        {
          char c = *(uint8 *)(UART0 + 0);
          consoleintr(c);
        }
      else if (irq == VIRTIO0_IRQ)
      {
        virtio_disk_intr();
      }
      else if (irq != 0)
        printf("Unexpected interrupt irq=%d\n", irq);

      if (irq != 0)
        *(uint32 *)PLIC_SCLAIM(hartid) = irq;

      break;
    }
    default:
      printf("sys_trap_handler: unknown interrupt irq=%d\n", irq);
      break;
    }
  }
  else
  {
    /* 同步异常：内核代码出了错，无法恢复，直接 panic */
    printf("sys_trap_handler: exception! scause=%ld, sepc=%p, stval=%p\n",
           scause, sepc, r_stval());
    panic("sys_trap_handler: unexpected exception");
  }

  /* ================================================================
   * 恢复 sepc 和 sstatus：
   * 某些情况下（如嵌套中断）它们可能被修改过，需要还原。
   * ================================================================ */
  w_sepc(sepc);
  w_sstatus(sstatus);
}

/* ================================================================
 * usertrap — 用户态陷阱处理（Lab6 新增）
 *
 * 当用户程序执行 ecall 时，CPU 切换到 S-Mode 并调用此函数。
 *
 * 区别于 sys_trap_handler：
 *   - 需要切换陷阱向量到 sys_trap_vector（防止用户态 PC 出现在栈跟踪里）
 *   - 需要将 epc 加 4，跳过 ecall 指令（否则返回后又会执行 ecall）
 *   - 只处理 scause == 8（来自 U-Mode 的 ecall）
 * ================================================================ */
void usertrap(void)
{
  /* 立即切换到内核态陷阱向量（防止处理用户陷阱时再发生用户态中断）*/
  w_stvec((uint64)sys_trap_vector);

  uint64 cause = r_scause();
  if (cause == 8)
  {
    intr_on();
    struct proc *p = myproc();
    w_stvec((uint64)sys_trap_vector);
    p->trapframe->epc += 4;
    intr_on();

    syscall();
    usertrap_finish();
  }
  else if (cause == 0x8000000000000001L)
  {
    w_sip(r_sip() & ~2);
    yield();
    usertrap_finish();
  }
  else if (cause == 0x8000000000000009L)
  {
    int hartid = r_tp();
    int irq = *(uint32 *)PLIC_SCLAIM(hartid);

    if (irq == UART0_IRQ)
      while ((*(uint8 *)(UART0 + 5) & 1) != 0)
      {
        char c = *(uint8 *)(UART0 + 0);
        consoleintr(c);
      }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq != 0)
      printf("Unexpected interrupt irq=%d\n", irq);

    if (irq != 0)
      *(uint32 *)PLIC_SCLAIM(hartid) = irq;
    usertrap_finish();
  }
  else
  {
    if (cause == 12)
    {
      struct proc *p = myproc();
      printf("\nPID = [%d]", p->pid);
      printf("\n 触发 Instruction Page Fault (cause = 12)\n");

      // sepc: CPU 案发时，正试图执行哪里的指令？
      printf("出事地址 (sepc) : 0x%lx\n", r_sepc());

      // stval: 硬件尝试访问却失败的虚拟地址（通常等于 sepc）
      printf("失败地址 (stval): 0x%lx\n", r_stval());

      // 查看我们分配给 userinit 的代码地址
      printf("预期的 initcode : 0x%lx\n", p->trapframe->epc);

      panic("Instruction Page Fault"); // 抓到现场后停机保护
    }
    if (cause == 15)
    {
      printf("\n[FATAL] Store Page Fault!\n");
      printf("sepc  (报错指令) = %p\n", r_sepc());
      printf("stval (报错地址) = %p\n", r_stval());
      panic("scause 15");
    }
    /* 理想情况下应该 exit(-1) 杀死该进程，暂不实现 */
    uint64 cause = r_scause();
    uint64 epc = r_sepc();
    uint64 tval = r_stval();
    printf("\n[USERTRAP 绝密档案] scause: %d, sepc: 0x%lx, stval: 0x%lx\n", cause, epc, tval);
    panic("usertrap");
  }
}
void usertrapret()
{
  intr_off();
  struct proc *p = myproc();

  // 1. 设置陷入入口
  w_stvec((uint64)uservec);

  // 2. 备好内核物资，供 uservec 进来的时候换装
  p->trapframe->kernel_sp = p->kstack + PGSIZE;
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_satp = (8ULL << 60) | (((uint64)kernel_pagetable) >> 12);

  // 🚨 核心锁死：把当前进程 trapframe 的地址刻进 sscratch！
  // 因为我们在 proc_pagetable 里把它恒等映射了，这个地址在内核态和用户态通用！
  w_sscratch((uint64)p->trapframe);

  // 3. 准备好特权级控制寄存器
  w_sstatus((r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE);
  w_sepc(p->trapframe->epc);

  // 4. 冲刷缓存
  __asm__ volatile("sfence.vma");
  __asm__ volatile("fence.i");

  // 5. 算出当前进程私有页表的 satp 格式，双参数调用传给汇编
  uint64 satp = (8ULL << 60) | (((uint64)p->pagetable) >> 12);
  userret((uint64)p->trapframe, satp);
}
