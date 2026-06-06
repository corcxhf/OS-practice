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
#include "elf.h"

extern struct cpu *mycpu(void);
extern void *memset(void *dst, int v, unsigned long n);
extern void *memmove(void *dest, const void *src, unsigned long n);
extern void consoleintr(int);
extern void console_print_char(char);

extern int argint(int, int *);
extern int argaddr(int, uint64 *);
extern int argstr(int, char *, int);
extern pagetable_t proc_pagetable(struct proc *p);
extern void proc_freepagetable(pagetable_t pagetable, uint64 sz);
extern void acquire(struct spinlock *);
extern void initlock(struct spinlock *lk, char *name);
extern void release(struct spinlock *lk);
extern int holding(struct spinlock *lk);

extern char *safepy(char *s, const char *t, int n);
extern int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
extern void wakeup(void *chan);
extern void forkret();
extern int filewrite(struct file *f, uint64 addr, int n);
extern void iunlockput(struct inode *);
extern void ilock(struct inode *);

extern int fetchaddr(uint64 addr, uint64 *ip);
extern int fetchstr(uint64 s, char *dst, int max);
extern int strlen(const char *s);
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
  // printf("Process [%d] exited with code [%d]\n", p->pid, p->xstate);
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
      console_print_char(buf[i]);
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

  // 1. 调用你原有的 allocproc，拿到子进程 proc 结构体，以及它自带的物理 trapframe
  struct proc *np = allocproc();
  if (np == 0)
    return -1;

  // 2. 分配内核栈 (kstack)
  if ((np->kstack = (uint64)kalloc()) == 0)
  {
    // 失败清理
    kfree(np->trapframe);
    acquire(&np->lock);
    np->status = TASK_FREE;
    release(&np->lock);
    return -1;
  }

  // 3. 设置子进程被调度器切出来时的内核现场
  np->context.sp = np->kstack + PGSIZE;
  np->context.ra = (uint64)forkret;

  // =======================================================================
  // 🚨 核心改造一：调用工厂，为子进程创建它专属的独立页表宇宙！
  // 这个工厂在内部已经帮子进程把 np->trapframe 映射进 np->pagetable 了！
  // =======================================================================
  np->pagetable = proc_pagetable(np);
  if (np->pagetable == 0)
  {
    kfree((void *)np->kstack);
    kfree(np->trapframe);
    acquire(&np->lock);
    np->status = TASK_FREE;
    release(&np->lock);
    return -1;
  }

  // 4. 正式拷贝 Trapframe 上下文（克隆父进程的用户态寄存器现场）
  // 此时 np->trapframe 已经是通过 allocproc 分配好的干净物理页
  *np->trapframe = *p->trapframe;

  // 5. 子进程在用户态的 fork 返回值必须设为 0
  np->trapframe->a0 = 0;

  // =======================================================================
  // 🚨 核心改造二：深度克隆用户内存 (uvmcopy)
  // 把父进程私有宇宙的代码、数据、栈，一页一页物理拷贝并映射给子进程的新页表
  // =======================================================================

  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    // 如果克隆失败，这里需要调用回收机制（先省略，优先跑通）
    return -1;
  }
  np->sz = p->sz;

  // 复制文件描述符
  for (int i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = p->ofile[i];

  // 7. 确立父子名分
  np->parent = p;

  // 8. 激活子进程，让调度器可以看得到它
  acquire(&np->lock);
  np->status = TASK_READY;
  release(&np->lock);

  return np->pid; // 父进程返回子进程的 PID
}

int kernel_strlen(const char *s)
{
  int len = 0;
  while (s[len])
    len++;
  return len;
}
uint64 sys_exec(void)
{
  char path[128];
  uint64 argv[MAXARG];
  int argc;
  uint64 uargv, uarg;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  struct proc *p = myproc();
  pagetable_t new_pagetable = 0;
  uint64 sz = 0, old_sz;
  pagetable_t old_pagetable;
  uint64 sp, stackbase;

  // =========================================================
  // 1. 从用户态读取参数（路径 path 和 参数数组 argv）
  // =========================================================
  // 获取第一个参数：待执行程序的路径（如 "/echo"）
  if (argstr(0, path, sizeof(path)) < 0)
    return -1;

  // 获取第二个参数：用户态的 argv 指针数组地址
  if (argaddr(1, &uargv) < 0)
    return -1;

  // 开始解析参数字符串，搬运到内核暂存
  memset(argv, 0, sizeof(argv));
  for (argc = 0; argc < MAXARG; argc++)
  {
    if (fetchaddr(uargv + sizeof(uint64) * argc, &uarg) < 0)
      goto bad;
    if (uarg == 0)
    {
      break; // 遇到 NULL 结束
    }
    // 分配内核内存暂存参数字符串
    argv[argc] = (uint64)kalloc();
    if (argv[argc] == 0)
      goto bad;
    if (fetchstr(uarg, (char *)argv[argc], PGSIZE) < 0)
      goto bad;
  }

  // =========================================================
  // 2. 查找并打开 ELF 文件，读取文件头 (ELF Header)
  // // =========================================================
  // begin_op();

  if ((ip = namei(path)) == 0)
  {
    // end_op();
    goto bad;
  }
  ilock(ip);

  // 读取 ELF 头部
  if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  // 校验 ELF 文件的魔数是否合法
  if (elf.magic != ELF_MAGIC)
    goto bad;

  // =========================================================
  // 3. 创世阶段：调用工厂申请一个属于该进程的全新隔离页表
  // =========================================================
  new_pagetable = proc_pagetable(p);
  if (new_pagetable == 0)
    goto bad;

  // =========================================================
  // 4. 解析 ELF 每一个 Segment 并加载代码/数据到新页表宇宙
  // =========================================================
  sz = 0;
  for (int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph))
  {
    if (readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;

    // 按照页面大小向下、向上对齐虚拟地址边界
    uint64 va_start = PGROUNDDOWN(ph.vaddr);
    uint64 va_end = PGROUNDUP(ph.vaddr + ph.memsz);

    // 逐页分配物理内存并建立映射
    for (uint64 a = va_start; a < va_end; a += PGSIZE)
    {
      void *pa = kalloc();
      if (pa == 0)
        goto bad;
      memset(pa, 0, PGSIZE); // 物理页清零

      // 🚨 铁律：你的专属参数顺序 (pagetable, pa, va, size, perm)
      // 赋予用户态权限 (PTE_U) 以及可读写执行权限
      if (mappages(new_pagetable, (uint64)pa, a, PGSIZE, PTE_R | PTE_W | PTE_X | PTE_U) < 0)
      {
        kfree(pa);
        goto bad;
      }
    }

    // 将磁盘上的段内容，精准读入刚刚映射好的物理内存页中
    uint64 seg_va = ph.vaddr;
    uint64 file_off = ph.off;
    uint64 file_sz = ph.filesz;

    while (file_sz > 0)
    {
      uint64 a = PGROUNDDOWN(seg_va);
      uint64 offset_in_page = seg_va - a;
      uint64 n = PGSIZE - offset_in_page;
      if (n > file_sz)
        n = file_sz;

      // 顺藤摸瓜：查当前的新页表，找到虚拟地址 a 对应的物理地址 pa
      pte_t *pte = walk(new_pagetable, a, 0);
      if (pte == 0 || (*pte & PTE_V) == 0)
        goto bad;
      uint64 pa = PTE2PA(*pte);

      // 直接从磁盘读入对应的物理页偏移地址
      if (readi(ip, 0, pa + offset_in_page, file_off, n) != n)
        goto bad;

      file_sz -= n;
      seg_va += n;
      file_off += n;
    }

    // 更新程序目前使用的虚拟空间最大水位线
    if (ph.vaddr + ph.memsz > sz)
      sz = ph.vaddr + ph.memsz;
  }

  iunlockput(ip);
  // end_op();
  ip = 0;

  // =========================================================
  // 5. 为新宇宙分配并映射用户栈 (User Stack)
  // =========================================================
  sz = PGROUNDUP(sz);
  void *stack_pa = kalloc();
  if (stack_pa == 0)
    goto bad;
  memset(stack_pa, 0, PGSIZE);

  // 🚨 映射用户栈：pa 在前，va (sz) 在后
  if (mappages(new_pagetable, (uint64)stack_pa, sz, PGSIZE, PTE_R | PTE_W | PTE_U) < 0)
  {
    kfree(stack_pa);
    goto bad;
  }

  stackbase = sz;
  sp = sz + PGSIZE; // 初始栈顶虚拟地址（Sv39 下往下生长）
  sz += PGSIZE;     // 内存大小增加一页

  // =========================================================
  // 6. 压栈环节：把暂存在内核的 argv 参数字符串推进新用户栈
  // =========================================================
  uint64 ustack[MAXARG + 1];
  for (int i = 0; i < argc; i++)
  {
    uint64 len = strlen((char *)argv[i]) + 1;
    sp -= len;
    sp -= sp % 16; // RISC-V 栈指针必须 16 字节对齐
    if (sp < stackbase)
      goto bad;

    // 映射虚拟栈地址到刚才分配的物理栈内存上
    // 因为内核在当前阶段是恒等映射，你可以像下面这样通过偏移直接写物理栈：
    memmove((void *)((uint64)stack_pa + (sp - stackbase)), (void *)argv[i], len);
    ustack[i] = sp; // 记录该参数在用户态的虚拟地址
  }
  ustack[argc] = 0; // argv[argc] = NULL

  // 把 ustack 数组（即各个参数的指针）也推入用户栈
  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if (sp < stackbase)
    goto bad;
  memmove((void *)((uint64)stack_pa + (sp - stackbase)), ustack, (argc + 1) * sizeof(uint64));

  // =========================================================
  // 7. 终极一换：新旧交替，换表仪式
  // =========================================================
  old_pagetable = p->pagetable;
  old_sz = p->sz;

  // 移交资产
  p->pagetable = new_pagetable;
  p->sz = sz;

  // 设置用户态起飞现场
  p->trapframe->epc = elf.entry; // 新程序的入口地址（如 0x100b0）
  p->trapframe->sp = sp;         // 刚刚布局好的用户栈顶虚拟地址
  p->trapframe->a1 = sp;         // 根据 RISC-V C ABI，a1 寄存器存放 argv 指针

  // 释放内核暂存参数时借用的临时物理页
  for (int i = 0; i < MAXARG; i++)
  {
    if (argv[i])
      kfree((void *)argv[i]);
  }
  // 释放老程序占用的旧页表和旧物理内存（防 OOM 核心）
  if (old_pagetable != 0)
  {
    proc_freepagetable(old_pagetable, old_sz);
  }
  return argc; // 成功返回！交由 usertrapret 里的 sret 顺滑切表落地！

bad:
  // 发生灾难，清理残留的半成品
  for (int i = 0; i < MAXARG; i++)
  {
    if (argv[i])
      kfree((void *)argv[i]);
  }
  if (new_pagetable)
    proc_freepagetable(new_pagetable, sz);
  if (ip)
  {
    iunlockput(ip);
    // end_op();
  }
  return -1;
}