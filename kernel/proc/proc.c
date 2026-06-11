/* proc.c — 进程管理（Lab5 任务1、3、4）
 *
 * 本文件实现了进程生命周期管理的核心逻辑：
 *   - procinit()   : 初始化进程表
 *   - allocproc()  : 为新进程分配 PCB
 *   - scheduler()  : 调度器主循环（无限轮询、找到就绪进程就运行）
 *   - yield()      : 当前进程主动放弃 CPU（配合时钟中断使用）
 */

#include "proc.h"
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include "fs.h"

extern void set_pte_u(pagetable_t pagetable, uint64 va);
struct proc proc[NPROC];
struct cpu cpus[NCPU];
extern struct file *filealloc();
extern void acquire(struct spinlock *);
extern void initlock(struct spinlock *lk, char *name);
extern void release(struct spinlock *lk);
extern int holding(struct spinlock *lk);
extern pagetable_t proc_pagetable(struct proc *p);
extern void *memmove(void *dest, const void *src, unsigned long n);

// static uint8 proczero_code[] = {
//     0x73,
//     0x00,
//     0x00,
//     0x00, // ecall
//     0x73,
//     0x00,
//     0x00,
//     0x00, // ecall
//     0x6f,
//     0x00,
//     0x00,
//     0x00, // j .
// };

/* * 声明由链接器 (ld -b binary) 自动生成的隐藏符号。
 * 命名规则是：_binary_文件名_扩展名_后缀
 * 因为我们在 Makefile 里生成的是 initcode.bin，所以名字如下：
 */
extern unsigned char _binary_initcode_bin_start[];
extern unsigned char _binary_initcode_bin_size[];

extern pagetable_t kernel_pagetable;
/* 进程 ID 计数器（每次 allocpid 返回后递增）*/
static int nextpid = 1;

/* ================================================================
 * mycpu — 获取当前 CPU 核心的 cpu 结构指针
 *
 * 实现方式：读取 tp 寄存器（在 start.c 中被设置为 hartid）
 * ================================================================ */
struct cpu *mycpu(void)
{
  int hartid = r_tp();
  return &cpus[hartid];
}

/* ================================================================
 * myproc — 获取当前 CPU 上正在运行的进程的 PCB 指针
 * ================================================================ */
struct proc *myproc(void) { return mycpu()->proc; }

/* ================================================================
 * allocpid — 分配一个唯一的进程 ID
 * ================================================================ */
int allocpid(void) { return nextpid++; }

void *memset(void *dst, int v, unsigned long n)
{
  unsigned char *p = (unsigned char *)dst;
  unsigned char val = (unsigned char)v;
  for (unsigned long i = 0; i < n; i++)
    p[i] = val;
  return dst;
}

/* ================================================================
 * procinit — 初始化进程表（内核启动时调用一次）
 *
 * 任务：将进程表中所有条目的状态初始化为 TASK_FREE。
 * ================================================================ */
void procinit(void)
{
  /* ================================================================
   * TODO [Lab5-任务1-步骤1]：
   *   遍历 proc[] 数组，将每个进程的 status 置为 TASK_FREE。
   * ================================================================ */
  for (int i = 0; i < NPROC; i++)
    proc[i].status = TASK_FREE, initlock(&proc[i].lock, "proclock");
}

/* ================================================================
 * allocproc — 在进程表中找一个空槽并初始化
 *
 * 返回：指向已初始化的 PCB 的指针；若进程表满，返回 0。
 *
 * 初始化内容：
 *   - 分配 pid
 *   - 将状态从 TASK_FREE 改为 TASK_ALLOCATED
 *   - 分配 trapframe 页（用于保存用户寄存器）
 *   - 初始化内核 context（ra 设为某个"进程首次被调度时跳入的地址"）
 * ================================================================ */
struct proc *allocproc(void)
{
  struct proc *p;

  /* 在进程表中寻找一个 TASK_FREE 的槽位 */
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->status == TASK_FREE)
      goto found;
    release(&p->lock);
  }
  return 0; /* 进程表已满 */

found:
  p->pid = allocpid();
  if ((p->trapframe = kalloc()) == 0)
  {
    p->status = TASK_FREE;
    return 0;
  }
  p->status = TASK_ALLOCATED;
  release(&p->lock);
  /* ================================================================
   * TODO [Lab5-任务1-步骤2]：
   *   完成进程初始化：
   *   1. 分配 pid：调用 allocpid()
   *   2. 分配 trapframe 页：调用 kalloc()；若失败则将状态恢复为 TASK_FREE 并返回0
   *   3. 将进程状态设为 TASK_ALLOCATED
   * ================================================================ */
  return p;
}

/* ================================================================
 * scheduler — 调度器主循环（永不返回！）
 *
 * 这是操作系统的"上帝"：它在所有进程之间无限轮转，
 * 当看到一个 TASK_READY 的进程时，就把 CPU 交给它。
 *
 * 流程：
 *   for 每次循环:
 *     1. 打开全局中断（防止系统无法接收时钟信号而死锁）
 *     2. 遍历进程表，找到 TASK_READY 的进程
 *     3. 将该进程标记为 TASK_RUNNING
 *     4. 调用 swtch，从调度器上下文切换到进程的内核上下文
 *     5. 当进程放弃 CPU（yield/sleep/exit）后，swtch 返回到这里
 *     6. 清除 mycpu()->proc，继续找下一个
 * ================================================================ */
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;

  for (;;)
  {
    /* 必须打开中断！否则时钟信号无法到达，调度无法触发 */
    intr_on();

    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->status == TASK_READY)
      {
        p->status = TASK_RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

void sched(void)
{
  struct proc *p = myproc();
  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->status == TASK_RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  swtch(&p->context, &mycpu()->context);
}

void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->status = TASK_READY;
  sched();
  release(&p->lock);
}

void forkret()
{
  release(&myproc()->lock);
  usertrapret();
}

void userinit()
{
  struct proc *p = allocproc();
  acquire(&p->lock);
  if (p == 0)
  {
    release(&p->lock);
    return;
  }
  if ((p->kstack = (uint64)kalloc()) == 0)
  {
    p->status = TASK_FREE;
    release(&p->lock);
    return;
  }

  uint64 initcode_size = (uint64)_binary_initcode_bin_size;
  p->pagetable = proc_pagetable(p);
  p->context.sp = p->kstack + PGSIZE;
  p->context.ra = (uint64)forkret;

  // 1. 申请存放 initcode 机器码的物理页
  uint64 code_pa = (uint64)kalloc();
  memset((void *)code_pa, 0, PGSIZE);
  memmove((void *)code_pa, _binary_initcode_bin_start, initcode_size);

  // 🚨 拨乱反正：不要再搞恒等映射了！
  // 强行把这页物理内存，映射到用户宇宙的绝对起点——虚拟地址 0x0 处！
  mappages(p->pagetable, code_pa, 0, PGSIZE, PTE_R | PTE_W | PTE_X | PTE_U);

  // 2. 分配并映射用户栈
  uint64 userstack_pa = (uint64)kalloc();
  memset((void *)userstack_pa, 0, PGSIZE);
  // 把用户栈安排在虚拟地址 0x1000 处（也就是紧挨着代码段的下一页）
  mappages(p->pagetable, userstack_pa, PGSIZE, PGSIZE, PTE_R | PTE_W | PTE_U);

  // 3. 冲刷 TLB
  asm volatile("sfence.vma zero, zero");
  memset(p->trapframe, 0, PGSIZE);

  // 🚨 终极解耦：用户态的眼睛从此只能看到低位虚拟地址！
  p->trapframe->epc = 0;              // 👈 新程序的入口永远是虚拟地址 0x0！
  p->trapframe->sp = PGSIZE + PGSIZE; // 👈 虚拟地址 0x2000 作为初始栈顶

  p->sz = PGSIZE + PGSIZE; // 👈 明确告诉系统，父进程现在有 2 页大小的用户空间！
  struct file *f0 = filealloc();
  struct file *f1 = filealloc();

  // f0->type = FD_CONSOLE; // 0 号是控制台（读键盘）
  // f1->type = FD_CONSOLE; // 1 号也是控制台（写屏幕）

  f0->type = FD_CONSOLE;
  f0->readable = 1; // 🚨 0号是标准输入，必须能读！
  f0->writable = 0; // 不能往键盘里写

  f1->type = FD_CONSOLE;
  f1->readable = 0; // 不能从屏幕里读
  f1->writable = 1;

  p->ofile[0] = f0;
  p->ofile[1] = f1;

  p->cwd = namei("/");
  p->name = "proczero";
  p->status = TASK_READY;
  release(&p->lock);
}

void wakeup(void *chan)
{
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p != myproc())
    {
      acquire(&p->lock);
      if (p->status == TASK_SLEEPING && p->chan == chan)
      {
        p->status = TASK_READY;
      }
      release(&p->lock);
    }
  }
}