/* vm.c — 虚拟内存与页表管理（Lab3 任务2-4）
 *
 * 本文件实现 RISC-V Sv39 三级分页机制：
 *   - walk()      : 在三级页表中查找（并按需分配中间级页表）
 *   - mappages()  : 批量建立虚拟地址到物理地址的映射
 *   - kvmininit() : 建立内核页表（映射内核各段和设备）
 *   - kvminithart(): 将页表写入 satp 寄存器，开启 MMU 分页
 *
 * 重要概念：
 *   虚拟地址（VA）→ MMU查页表 → 物理地址（PA）
 *   Sv39中VA分解：[38:30]=VPN[2], [29:21]=VPN[1], [20:12]=VPN[0],
 * [11:0]=页内偏移
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include "proc.h"

/* 内核根页表（全局变量，Lab3 建立后整个内核都使用它）*/
pagetable_t kernel_pagetable;

/* 外部符号（由链接脚本/编译器生成，标记内核各段边界）*/
extern char etext[];       /* 内核代码段结束地址 */
extern char end_address[]; /* 内核数据段结束地址 */
extern void userret(uint64, uint64);
extern void *memcpy(void *dst, const void *src, unsigned long n);
extern void *memmove(void *dest, const void *src, unsigned long n);
extern void *memset(void *dst, int v, unsigned long n);
extern void uservec();
extern int strlen(const char *s);
extern int fetchaddr(uint64 addr, uint64 *ip);
/* ================================================================
 * walk — 在三级页表中查找虚拟地址 va 对应的最终 PTE 指针
 *
 * 参数：
 *   pagetable — 根页表物理地址
 *   va        — 要查找的虚拟地址
 *   alloc     — 若中间级页表不存在：1=自动分配新页表，0=直接返回0
 *
 * 返回值：最底层（level-0）PTE 的指针；找不到时返回 0。
 *
 * Sv39 三级页表遍历（从 level-2 到 level-0）：
 *   每级用 PX(level, va) 提取 9 位索引，乘以8字节，找到对应 PTE。
 *   PTE 中取出物理页号（PPN），转为物理地址（下一级页表基地址）。
 * ================================================================ */
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk: virtual address out of range");
  for (int level = 2; level > 0; level--)
  {
    pte_t *pte = &pagetable[PX(level, va)];

    if (*pte & PTE_V)
      pagetable = (pagetable_t)PTE2PA((uint64)*pte);

    else
    {
      if (!alloc)
        return 0; /* 不允许分配，返回失败 */

      pagetable = (pagetable_t)kalloc();
      if (pagetable == 0)
        return 0; /* 内存耗尽 */

      for (char *p = (char *)pagetable; p < (char *)pagetable + PGSIZE; p++)
        *p = 0;

      *pte = PA2PTE(pagetable);
      *pte |= PTE_V;
    }
  }

  return &pagetable[PX(0, va)];
}

/* ================================================================
 * mappages — 建立从虚拟地址范围到物理地址范围的映射
 *
 * 参数：
 *   pagetable — 根页表
 *   pa        — 对应的物理地址起始
 *   va        — 虚拟地址起始（会被向下对齐到页边界）
 *   size      — 映射大小（字节）
 *   perm      — 权限位（PTE_R/PTE_W/PTE_X/PTE_U 的组合）
 *
 * 返回值：0 表示成功，-1 表示失败（内存不足）
 * ================================================================ */
int mappages(pagetable_t pagetable, uint64 pa, uint64 va, uint64 size,
             int perm)
{
  uint64 a, last;
  pte_t *pte;

  if (size == 0)
    panic("mappages: size is 0");

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);

  for (;;)
  {
    /* 找到 va=a 对应的 level-0 PTE（必要时分配中间页表）*/
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;

    /* 重复映射是内核 Bug */
    if (*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | (perm | PTE_V);

    if (a == last)
      break;

    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

/* ================================================================
 * kvmininit — 建立内核页表
 *
 * 建立的映射关系（"恒等映射"：虚拟地址 = 物理地址，方便内核访问）：
 *   UART0   设备 → 可读写
 *   内核代码段   → 可读+可执行
 *   内核数据段   → 可读+可写
 *   可用物理内存 → 可读+可写
 *
 * 注：更完整的版本还需映射 PLIC、virtio 等设备（Lab7使用）。
 * ================================================================ */
void kvmininit(void)
{
  /* 分配根页表 */
  kernel_pagetable = (pagetable_t)kalloc();
  if (kernel_pagetable == 0)
    panic("kvmininit: out of memory");

  /* 清零根页表 */
  for (int i = 0; i < PGSIZE / 8; i++)
    ((uint64 *)kernel_pagetable)[i] = 0;

  /* ================================================================
   * TODO [Lab3-任务4-步骤1]：
   *   映射 UART0 串口设备（MMIO区域），使内核可以访问串口寄存器。
   *   地址：UART0（见 memlayout.h），大小：PGSIZE，权限：可读+可写。
   * ================================================================ */
  mappages(kernel_pagetable, 0x10001000, 0x10001000, PGSIZE, PTE_R | PTE_W);
  mappages(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  mappages(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  /* ================================================================
   * TODO [Lab3-任务4-步骤2]：
   *   映射内核代码段：从 KERNBASE 到 etext。
   *   权限：可读+可执行（注意：代码段不能有写权限！）。
   * ================================================================ */
  mappages(kernel_pagetable, KERNBASE, KERNBASE, PGROUNDUP((uint64)etext) - KERNBASE, PTE_R | PTE_X);
  /* ================================================================
   * TODO [Lab3-任务4-步骤3]：
   *   映射内核数据段和剩余可用物理内存：从 etext 到 PHYSTOP。
   *   权限：可读+可写（数据段需要写权限，但不能有可执行权限）。
   * ================================================================ */
  mappages(kernel_pagetable, PGROUNDUP((uint64)etext), PGROUNDUP((uint64)etext), (uint64)PHYSTOP - PGROUNDUP((uint64)etext), PTE_R | PTE_W);
}

void kvminithart(void)
{
  w_satp(MAKE_SATP(kernel_pagetable));

  sfence_vma();
  /* ================================================================
   * TODO [Lab3-任务4-步骤4]：
   *   1. 将根页表地址写入 satp 寄存器（开启Sv39分页）。
   *      使用 MAKE_SATP 宏将根页表物理地址转为 satp 的格式。
   *   2. 刷新 TLB（清空CPU中缓存的旧地址翻译结果）：sfence_vma()。
   *   注意：这两步顺序不能颠倒！
   * ================================================================ */
}

void set_pte_u(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("set_pte_u: pte should exist");
  *pte |= PTE_U | PTE_R | PTE_X | PTE_W;
}

void *memmove(void *dest, const void *src, unsigned long n)
{
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  if (d == s || n == 0)
    return dest;
  if (d < s)
    for (unsigned long i = 0; i < n; i++)
      d[i] = s[i];
  else
    for (unsigned long i = n; i > 0; i--)
      d[i - 1] = s[i - 1];
  return dest;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
  return memmove(dst, src, n);
}

int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint64 flags;
  char *mem;

  // 获取当前子进程和父进程的核心生命通道边界（物理/虚拟地址）
  // 确保拷贝时把它们当成禁区保护起来
  uint64 trapframe_va = PGROUNDDOWN((uint64)myproc()->trapframe);
  uint64 userret_va = PGROUNDDOWN((uint64)userret);

  for (i = 0; i < sz; i += PGSIZE)
  {
    // 🚨 终极防火墙：如果当前遍历到的虚拟地址是 trapframe 或者 userret 的特殊页面
    // 证明工厂在创建进程时已经独立映射过了，子进程不需要、也绝对不能重复拷贝！直接跳过！
    if (i == trapframe_va || i == userret_va)
    {
      continue;
    }

    if ((pte = walk(old, i, 0)) == 0)
      continue; // 如果父进程这页本来就是空的，直接跳过
    if ((*pte & PTE_V) == 0)
      continue;

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    if ((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char *)pa, PGSIZE);

    // 严丝合缝的参数顺序
    if (mappages(new, (uint64)mem, i, PGSIZE, flags) < 0)
    {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  return -1;
}

uint64 walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  // 1. 调用你现有的 walk，不要分配新页 (alloc = 0)
  pte = walk(pagetable, va, 0);

  // 2. 检查 PTE 是否存在，以及是否有效 (PTE_V)
  // 同时必须检查 PTE_U 位，确保这是用户有权访问的地址
  if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
    return 0;

  // 3. 从 PTE 中提取物理页号 (PPN) 并加上页内偏移
  // PTE 的高 44 位（对于 Sv39）是 PPN
  pa = PTE2PA(*pte) | (va & (PGSIZE - 1));

  return pa;
}

pagetable_t proc_pagetable(struct proc *p)
{
  // 1. 申请一个物理页作为一级根页表
  pagetable_t pgtbl = (pagetable_t)kalloc();
  if (pgtbl == 0)
    return 0;

  // 务必清零！保证里面没有任何残留垃圾
  memset(pgtbl, 0, PGSIZE);

  uint64 userret_pa = PGROUNDDOWN((uint64)userret);
  if (mappages(pgtbl, userret_pa, userret_pa, PGSIZE, PTE_R | PTE_X) < 0)
  {
    return 0;
  }
  uint64 uservec_pa = PGROUNDDOWN((uint64)uservec);
  if (uservec_pa != userret_pa)
  {
    mappages(pgtbl, uservec_pa, uservec_pa, PGSIZE, PTE_R | PTE_X);
  }

  // ==========================================================
  // 3. 映射生命通道二：陷阱帧 (trapframe 所在的物理页)
  // userret 汇编需要通过 a0 寄存器去读取这里的寄存器数据
  // 权限必须是 可读 + 可写 (PTE_R | PTE_W)
  // ==========================================================
  uint64 tf_pa = PGROUNDDOWN((uint64)p->trapframe);
  if (mappages(pgtbl, tf_pa, tf_pa, PGSIZE, PTE_R | PTE_W) < 0)
  {
    return 0;
  }

  return pgtbl;
}

// =======================================================================
// 1. 解除虚拟地址映射，并选择性释放对应的物理内存页
// =======================================================================
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  // 严格按页大小对齐检查
  if ((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  // 遍历每一页进行解除映射
  for (a = va; a < va + npages * PGSIZE; a += PGSIZE)
  {
    // walk 查找对应的页表项，第三个参数为 0 表示找不到不用创建新的
    if ((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");

    // 检查这个映射是否有效
    if ((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");

    if (PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");

    // 如果指定了 do_free = 1，就说明需要把这块物理砖头还给系统
    if (do_free)
    {
      uint64 pa = PTE2PA(*pte);
      kfree((void *)pa);
    }

    // 彻底抹除这个页表项，斩断红线
    *pte = 0;
  }
}

// =======================================================================
// 2. 递归销毁页表树的中间节点（释放页表页面本身占用的内存）
// =======================================================================
void freewalk(pagetable_t pagetable)
{
  // 一个页表页里有 512 + 1 个页表项
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = pagetable[i];

    // 如果该项有效，且它指向的是下一级页表（而不是物理叶子页）
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
    {
      // 顺藤摸瓜，递归进入下一级页表
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    }
    else if (pte & PTE_V)
    {
      // 如果到了叶子节点（代码/栈）却还没被 uvmunmap 清理干净，说明前面漏了
      panic("freewalk: leaf");
    }
  }
  // 销毁完所有的子节点后，把自己这一页的账本页面也释放掉
  kfree((void *)pagetable);
}

// =======================================================================
// 3. 终极打包函数：供 sys_exec 调用的完整销毁入口
// =======================================================================
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  // 拿到当前正在执行 exec 的进程控制块
  struct proc *cur_p = myproc();
  // 算出当前进程绝对不能被释放的 trapframe 物理页地址
  uint64 forbidden_pa = PGROUNDDOWN((uint64)cur_p->trapframe);

  for (int i = 0; i < 512; i++)
  {
    pte_t pte1 = pagetable[i];
    if (pte1 & PTE_V)
    {
      if (pte1 & (PTE_R | PTE_W | PTE_X))
      {
        pagetable[i] = 0;
        continue;
      }

      pagetable_t pgtbl2 = (pagetable_t)PTE2PA(pte1);
      for (int j = 0; j < 512; j++)
      {
        pte_t pte2 = pgtbl2[j];
        if (pte2 & PTE_V)
        {
          if (pte2 & (PTE_R | PTE_W | PTE_X))
          {
            pgtbl2[j] = 0;
            continue;
          }

          pagetable_t pgtbl3 = (pagetable_t)PTE2PA(pte2);
          for (int k = 0; k < 512; k++)
          {
            pte_t pte3 = pgtbl3[k];
            if ((pte3 & PTE_V) && (pte3 & (PTE_R | PTE_W | PTE_X)))
            {

              uint64 pa = PTE2PA(pte3);

              // 🚨【终极特赦令】：如果是用户空间或者高端资产，允许释放
              if ((pte3 & PTE_U) || (pa >= 0x87000000))
              {
                // 1. 严格避开内核核心代码区 (0x80000000 ~ 0x80100000)
                // 2. 严格避开当前进程正在使用的、保命用的 trapframe 物理页！
                if ((pa < 0x80000000 || pa >= 0x80100000) && (pa != forbidden_pa))
                {
                  kfree((void *)pa); // 只有安全的私有资产才允许释放
                }
              }
              pgtbl3[k] = 0; // 抹黑表项
            }
          }
        }
      }
    }
  }

  asm volatile("sfence.vma zero, zero");
  freewalk(pagetable);
}