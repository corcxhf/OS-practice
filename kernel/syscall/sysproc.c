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
#include "fs.h"

extern int namecmp(const char *s, const char *t);
extern struct file *filealloc();
extern void fileclose(struct file *f);
extern int fdalloc(struct file *f);
extern struct file *filedup(struct file *f);
extern struct cpu *mycpu(void);
extern void *memset(void *dst, int v, unsigned long n);
extern void *memmove(void *dest, const void *src, unsigned long n);
extern void consoleintr(int);
extern void console_print_char(char);
struct inode *idup(struct inode *ip);
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
extern int filestat(struct file *f, uint64 addr);
extern int pipewrite(struct pipe *pi, uint64 addr, int n);
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
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      fileclose(p->ofile[fd]); // 引用计数减1，真正触发管道读端的 EOF！
      p->ofile[fd] = 0;
    }
  }
  if (p->cwd)
  {
    iput(p->cwd);
    p->cwd = 0;
  }

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
  uint64 addr;
  struct file *f;
  struct proc *p = myproc();
  if (argint(0, &fd) < 0 || argint(2, &len) < 0 || argaddr(1, &addr) < 0)
    return -1;

  if (fd < 0 || fd >= 16 || (f = p->ofile[fd]) == 0)
    return -1;

  if (f->writable == 0)
    return -1;

  // ==========================================================
  // 🚨 核心战术：无论是屏幕还是文件，统一先搬到内核避险！
  // ==========================================================
  char buf[128];
  int copy_len = len < 127 ? len : 127;
  uint64 user_addr = addr;

  for (int i = 0; i < copy_len; i++)
  {
    // 1. 算出行首地址和偏移量
    uint64 va_page = PGROUNDDOWN(user_addr);
    uint64 offset = user_addr - va_page;

    // 2. 拿用户进程的页表去查！
    pte_t *pte = walk(p->pagetable, va_page, 0);
    if (pte == 0 || (*pte & PTE_V) == 0)
    {
      return -1; // 发现非法地址，立刻拦截，保护内核！
    }

    // 3. 算出内核可以直接访问的高端物理地址
    uint64 pa = PTE2PA(*pte);
    char *kernel_ptr = (char *)(pa + offset);

    // 4. 将真实的物理内存数据，一滴不漏地搬进内核的救生艇 buf
    buf[i] = *kernel_ptr;
    user_addr++;
  }

  // ==========================================================
  // 🚨 多态分发：现在大家用的都是安全的内核指针了！
  // ==========================================================
  if (f->type == FD_CONSOLE)
  {
    for (int i = 0; i < copy_len; i++)
      console_print_char(buf[i]);
    return copy_len;
  }
  else if (f->type == FD_INODE)
  {
    int ret = filewrite(f, (uint64)buf, copy_len);
    return ret;
  }
  else if (f->type == FD_PIPE)
  {
    return pipewrite(f->pipe, (uint64)buf, copy_len);
  }

  char msg4[] = "[Trace: UNKNOWN file type!]\n";
  for (int i = 0; msg4[i]; i++)
    console_print_char(msg4[i]);
  return -1;
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

  *np->trapframe = *p->trapframe;
  np->trapframe->a0 = 0;

  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    // 如果克隆失败，这里需要调用回收机制（先省略，优先跑通）
    return -1;
  }
  np->sz = p->sz;

  // 复制文件描述符
  for (int i = 0; i < NOFILE; i++)
  {
    if (p->ofile[i])
    {
      np->ofile[i] = p->ofile[i];
      // ==========================================================
      // 🚨 终极修复：必须增加文件的引用计数！
      // ==========================================================
      filedup(p->ofile[i]);
    }
  }

  // 7. 确立父子名分
  np->parent = p;
  if (p->cwd)
    np->cwd = idup(p->cwd);
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
  if (argstr(0, path, sizeof(path)) < 0)
    return -1;
  if (argaddr(1, &uargv) < 0)
    return -1;
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

uint64 sys_fstat(void)
{
  struct file *f;
  uint64 st; // 用户传进来的结构体虚拟地址
  int fd;

  // 1. 获取参数 fd 和 用户地址 st
  if (argint(0, &fd) < 0 || argaddr(1, &st) < 0)
    return -1;

  // 2. 检查 fd 合法性，拿到文件结构体
  if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
    return -1;

  // 3. 调用底层的 filestat 获取信息
  return filestat(f, st);
}

uint64 sys_mkdir(void)
{
  char path[MAXPATH];
  char name[DIRSIZ];
  struct inode *dp;
  struct inode *ip;

  /* 1. 从 trapframe 读取路径参数 */
  if (argstr(0, path, MAXPATH) < 0)
    return -1;

  /* 2. 调用 nameiparent() 分离出父目录路径 dp 和要创建的目录名 name */
  if ((dp = nameiparent(path, name)) == 0)
    return -1;

  ilock(dp);

  /* 3. 在父目录中查找是否已存在同名条目（防重名） */
  if ((ip = dirlookup(dp, name, 0)) != 0)
  {
    /* 若已存在同名文件或目录，创建失败 */
    iunlockput(dp);
    ilock(ip);
    iunlockput(ip); // 释放 dirlookup 增加的引用计数与锁
    return -1;
  }

  /* 4. 若不存在：分配新 inode，注意此处类型是 T_DIR */
  if ((ip = ialloc(dp->dev, T_DIR)) == 0)
  {
    iunlockput(dp);
    return -1;
  }

  ilock(ip);

  /* 5. 初始化新目录的链接数 */
  // 自身初始为 2 (父目录的条目 + 自身的 ".")
  ip->nlink = 2;

  /* 6. 核心连招：在新目录内部写入 "." 和 ".." 两个拓扑锚点 */
  // "." 指向新目录自己 (ip->inum)
  // ".." 指向父目录 (dp->inum)
  if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
  {
    iunlockput(ip);
    iunlockput(dp);
    return -1;
  }

  /* 7. 将新目录的名字和号注册到父目录中 */
  if (dirlink(dp, name, ip->inum) < 0)
  {
    iunlockput(ip);
    iunlockput(dp);
    return -1;
  }

  /* 8. 父目录的链接数加 1 (因为子目录里的 ".." 正深深地看着它) */
  dp->nlink++;

  /* 9. 将新目录和父目录的最新状态同步到磁盘 */
  iupdate(dp);
  iupdate(ip);

  /* 10. 功成身退，释放父子目录的锁和引用 */
  iunlockput(dp);
  iunlockput(ip);

  return 0;
}

uint64 sys_chdir(void)
{
  char path[128]; // 或者你的 MAXPATH
  struct inode *ip;
  struct proc *p = myproc();

  // 1. 获取路径参数，并用刚刚升级的 namei 找到目标 Inode
  if (argstr(0, path, sizeof(path)) < 0 || (ip = namei(path)) == 0)
  {
    return -1; // 路径不存在
  }

  // 2. 锁住 Inode，检查它到底是不是个目录
  ilock(ip);
  if (ip->type != T_DIR)
  {
    iunlockput(ip); // 不是目录（比如是普通文件），拒绝切换！
    return -1;
  }
  iunlock(ip); // 检查完毕，解锁（但保留引用计数，因为接下来要把它当 cwd 用）

  // 3. 辞旧迎新：释放旧的目录，换上新的目录！
  iput(p->cwd);
  p->cwd = ip;

  return 0;
}

uint64 sys_pipe(void)
{
  uint64 fdarray; // 用户态传进来的 int fd[2] 的虚拟地址
  struct proc *p = myproc();
  struct file *rf = 0, *wf = 0;
  int fd0 = -1, fd1 = -1;
  struct pipe *pi = 0;

  // 1. 抓取用户态参数：获取存放两个 fd 的数组首地址
  if (argaddr(0, &fdarray) < 0)
    return -1;

  // 2. 挖坑：为管道环形缓冲区分配一页物理内存
  // 假设你的内存分配器是 kalloc()
  pi = (struct pipe *)kalloc();
  if (pi == 0)
    goto bad;

  // 初始化管道内部的锁和状态
  initlock(&pi->lock, "pipe");
  pi->nread = 0;
  pi->nwrite = 0;
  pi->readopen = 1;
  pi->writeopen = 1;

  // 3. 铸造：从内核全局文件表中分配两个 struct file 对象
  // 假设你拥有标准的 filealloc()
  if ((rf = filealloc()) == 0 || (wf = filealloc()) == 0)
    goto bad;

  // 配置读端文件结构
  rf->type = FD_PIPE;
  rf->readable = 1;
  rf->writable = 0;
  rf->pipe = pi;

  // 配置写端文件结构
  wf->type = FD_PIPE;
  wf->readable = 0;
  wf->writable = 1;
  wf->pipe = pi;

  // 4. 分发：动用你修复好的 fdalloc，占用当前进程的 process table 槽位
  if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0)
    goto bad;

  // 5. 偷渡：将内核生成的 fd0 和 fd1，塞回用户态的 fdarray[0] 和 fdarray[1]
  // 这里我们用你最拿手的、绝对安全的 walk 查表逐字节（或4字节）搬运法！
  int fds[2];
  fds[0] = fd0;
  fds[1] = fd1;

  char *src = (char *)fds;
  uint64 dst_va = fdarray;
  for (int i = 0; i < sizeof(fds); i++)
  {
    uint64 va_page = PGROUNDDOWN(dst_va);
    uint64 offset = dst_va - va_page;

    pte_t *pte = walk(p->pagetable, va_page, 0);
    if (pte == 0 || (*pte & PTE_V) == 0)
      goto bad;

    uint64 pa = PTE2PA(*pte);
    char *kernel_ptr = (char *)(pa + offset);

    *kernel_ptr = src[i]; // 稳稳写入用户地址
    dst_va++;
  }

  return 0; // 成功！

bad:
  // 灾难清理：如果中间任何一步挂了，必须把前面申请的资源全部吐出来，防止内存泄漏
  if (pi)
    kfree((void *)pi);
  if (rf)
  {
    if (fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
  }
  if (wf)
  {
    if (fd1 >= 0)
      p->ofile[fd1] = 0;
    fileclose(wf);
  }
  return -1;
}

uint64 sys_dup(void)
{
  struct file *f;
  int oldfd;
  int newfd;
  struct proc *p = myproc();

  // 1. 从用户态拿到要复制的旧 fd
  if (argint(0, &oldfd) < 0)
    return -1;

  // 2. 检查这个旧 fd 是不是合法的，以及它到底有没有打开文件
  if (oldfd < 0 || oldfd >= NOFILE || (f = p->ofile[oldfd]) == 0)
    return -1;

  // 3. 召唤你刚刚修复的无敌 fdalloc！
  // 它会从 0 开始遍历，找到最小的空位，把 f 塞进去
  if ((newfd = fdalloc(f)) < 0)
    return -1;

  // 4. 极其关键的一步：增加文件引用计数！
  filedup(f);

  // 5. 返回新分配的槽位编号
  return newfd;
}

uint64 sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint offset;

  // 1. 从用户态获取要删除的文件路径
  if (argstr(0, path, MAXPATH) < 0)
    return -1;

  // 2. 找到该文件的“父目录 Inode (dp)” 和“目标文件名 (name)”
  // nameiparent 会解析路径，返回父目录的 Inode 指针
  if ((dp = nameiparent(path, name)) == 0)
    return -1;

  ilock(dp);

  // 🚨 绝对防御：连根拔起是大忌，不能删除当前目录(.)和上级目录(..)
  if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  // 3. 在父目录的条目中查找目标文件，拿到它的 Inode (ip) 和所在偏移量 (offset)
  if ((ip = dirlookup(dp, name, &offset)) == 0)
    goto bad;
  ilock(ip);

  // 🚨 防御检查：如果目标是一个目录，必须保证它是空目录才能删（防止孤儿目录树挂在虚空中）
  if (ip->type == T_DIR)
  {
    // 建议：如果你还没实现 isdirempty 检查，这里可以直接粗暴地拒绝删除目录
    // goto bad_unlock;
  }

  // 4. 物理抹除：用一个空的、全为 0 的 dirent 结构体，覆盖掉父目录里关于这个文件的记录
  memset(&de, 0, sizeof(de));
  if (writei(dp, 0, (uint64)&de, offset, sizeof(de)) != sizeof(de))
    panic("unlink: writei");

  // 5. 核心降维：如果它不是目录，或者你允许删除目录，将其硬链接数 -1
  if (ip->type == T_DIR)
  {
    dp->nlink--;
    iupdate(dp);
  }
  ip->nlink--;
  iupdate(ip); // 把修改后的 Inode 状态刷入磁盘

  // 6. 优雅释放：解锁并归还 Inode。
  // 如果此时 ip->nlink == 0，iunlockput 内部的 iput 逻辑会自动去释放磁盘 Block 块！
  iunlockput(ip);
  iunlockput(dp);

  return 0;

bad:
  iunlockput(dp);
  return -1;
}