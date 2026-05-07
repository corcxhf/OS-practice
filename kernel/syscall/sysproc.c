/* sysproc.c — 系统调用内核实现（Lab6 任务4）
 *
 * 每个 sys_xxx() 函数是对应系统调用的真正内核实现。
 * 它们不接受参数（参数通过陷阱帧的寄存器传入，用 argint/argaddr 读取），
 * 返回 uint64 类型的结果值。
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"

extern struct cpu *mycpu(void);

extern void consoleintr(int);

extern int argint(int, int *);
extern int argaddr(int, uint64 *);
extern int argstr(int, char *, int);

extern void acquire(struct spinlock *);
extern void initlock(struct spinlock *lk, char *name);
extern void release(struct spinlock *lk);
extern int holding(struct spinlock *lk);

extern char *safepy(char *s, const char *t, int n);
extern int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
extern void wakeup(void *chan);
extern void forkret();

extern int filewrite(struct file *f, uint64 addr, int n);
/* ================================================================
 * sys_getpid — 返回当前进程的 PID
 *
 * 对应的用户接口：int getpid(void)
 *
 * 实现很简单：调用 myproc() 获取当前进程的 PCB，
 * 然后返回它的 pid 字段。
 * ================================================================ */
uint64 sys_getpid(void)
{
  return myproc()->pid;
}

/* ================================================================
 * sys_exit (Lab6 扩展)
 *   实现进程退出。简化版：打印退出信息，将进程状态设为 TASK_ZOMBIE，然后切回调度器。
 * ================================================================ */

/* ================================================================
 * TODO [Lab6-任务4-步骤3（进阶）]：
 *   实现简化版 sys_write：
 *   1. int fd = myproc()->trapframe->a0;
 *   2. 获取出参 n（系统调用的第一个参数，可用 argint 拿取），并赋给 p->xstate
 *   3. 打印类似 "Process [pid] exited with code [n]\n"
 *   4. 设置 p->status = TASK_ZOMBIE
 *   5. 调用 swtch 切回调度器：swtch(&p->context, &mycpu()->context);
 * ================================================================ */

uint64 sys_exit(void)
{
  struct proc *p = myproc();
  argint(0, &p->xstate);
  // 1. 记录退出码
  p->status = TASK_ZOMBIE;
  printf("Process [%d] exited with code [%d]\n", p->pid, p->xstate);
  // 2. 【核心修复】：唤醒正在等待的父进程
  // 这里必须唤醒 p->parent，且通道必须跟 wait 里的 sleep 一致
  wakeup(p->parent);

  // 3. 切换到调度器，永远不再回来
  acquire(&p->lock);
  sched();
  panic("exit reached unreachable code");
}

/* ================================================================
 * sys_write — 向文件描述符写数据
 *
 * 用户接口：int write(int fd, const void *buf, int count)
 * 参数从陷阱帧读取：
 *   fd    = trapframe->a0
 *   buf   = trapframe->a1（用户虚拟地址，不能直接在内核读！）
 *   count = trapframe->a2
 *
 * 简化版：如果 fd==1（标准输出），直接把字符打印到串口。
 * ================================================================ */
uint64 sys_write(void)
{
  int fd, len;
  char buf[128];
  struct file *f;

  argint(0, &fd);
  argint(2, &len);

  // 1. 【核心修复】：开启内核访问用户内存的权限
  uint64 sstatus = r_sstatus();     // 读取当前的 sstatus
  w_sstatus(sstatus | SSTATUS_SUM); // 临时把 SUM 置为 1

  // 2. 现在执行读取操作就不会报 scause=13 了
  argstr(1, buf, len + 1);

  // 3. 【核心修复】：读取完必须立刻恢复，把墙重新砌好
  w_sstatus(sstatus);

  if (fd == 1)
  {
    for (int i = 0; i < len; i++)
    {
      // 确保你的 console 输出函数是正确的（比如 uartputc）
      consoleintr(buf[i]);
    }
    return 0;
  }

  f = myproc()->ofile[fd];

  // 调用之前写好的 filewrite，它内部会处理 ilock/writei/iunlock
  return filewrite(f, (uint64)buf, (int)len);
}

void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // 【核心修复】：防止递归死锁
  // 如果 lk 就是 &p->lock，说明外层已经 acquire 过了，直接跳过
  if (lk != &p->lock)
  {
    acquire(&p->lock);
    release(lk);
  }

  p->status = TASK_SLEEPING;
  p->chan = chan;

  sched(); // 切换到调度器，调度器在 swtch 后会释放 p->lock

  // --- 醒来后 ---
  p->chan = 0;

  // 恢复锁的状态：如果是同一个锁，也不用处理
  if (lk != &p->lock)
  {
    release(&p->lock);
    acquire(lk);
  }
}
uint64 sys_wait(void)
{
  struct proc *p = myproc();
  struct proc *son;
  int have_kids, pid;
  uint64 addr;
  argaddr(0, &addr);

  // xv6 的标准做法：在整个循环外拿父进程锁
  acquire(&p->lock);

  for (;;)
  {
    have_kids = 0;
    for (son = proc; son < &proc[NPROC]; son++)
    {
      if (son->parent == p)
      {
        acquire(&son->lock);
        have_kids = 1;
        if (son->status == TASK_ZOMBIE)
        {
          pid = son->pid;
          if (addr != 0)
          {
            // ... 写回 son->xstate ...
          }
          son->status = TASK_FREE;
          son->pid = 0;
          son->parent = 0;

          release(&son->lock);
          release(&p->lock);
          return pid;
        }
        release(&son->lock);
      }
    }

    if (!have_kids || p->killed)
    {
      release(&p->lock);
      return -1;
    }

    // 此时 p->lock 是持有的，sleep 内部会处理锁的释放
    sleep(p, &p->lock);
  }
}

uint64 sys_fork(void)
{
  struct proc *p = myproc();
  struct proc *np = allocproc();
  if (np == 0)
    return -1;

  // 1. 【补救分配】：必须给子进程一个物理页存陷阱帧
  // 如果不写这行，np->trapframe 就是 0，下一行拷贝时必崩
  if ((np->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    return -1;
  }

  // 2. 【补救分配】：必须给子进程一个独立的内核栈
  if ((np->kstack = (uint64)kalloc()) == 0)
  {
    kfree(np->trapframe);
    return -1;
  }

  // 3. 【现场初始化】：设置子进程醒来后的样子
  np->context.sp = np->kstack + PGSIZE;
  np->context.ra = (uint64)forkret;

  // 4. 【正式拷贝】：现在 np->trapframe 有地址了，可以拷了
  *np->trapframe = *p->trapframe;

  // 5. 子进程返回值设为 0
  np->trapframe->a0 = 0;

  // 6. 复制用户内存 (uvmcopy)
  // 这里要注意：如果还在用 kernel_pagetable，uvmcopy 里的映射会产生冲突
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    return -1;
  }
  np->sz = p->sz;

  // 7. 父子关系
  np->parent = p;

  // 8. 激活子进程
  acquire(&np->lock);
  np->status = TASK_READY;
  release(&np->lock);

  return np->pid;
}