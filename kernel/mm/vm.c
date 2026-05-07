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

/* 内核根页表（全局变量，Lab3 建立后整个内核都使用它）*/
pagetable_t kernel_pagetable;

/* 外部符号（由链接脚本/编译器生成，标记内核各段边界）*/
extern char etext[];       /* 内核代码段结束地址 */
extern char end_address[]; /* 内核数据段结束地址 */

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
  uint flags;
  char *mem;

  // 以页面（4KB）为步长，遍历父进程的所有内存
  for (i = 0; i < sz; i += PGSIZE)
  {
    // 1. 找到父进程该虚拟地址对应的页表项 (PTE)
    if ((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");

    // 2. 检查该页是否有效 (PTE_V)
    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");

    // 3. 提取父进程的物理地址 (PA) 和 权限位 (Flags)
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // 4. 为子进程申请一个新的物理页
    if ((mem = kalloc()) == 0)
      goto err;

    // 5. 将父进程物理页的内容完整拷贝到子进程的新页中
    memmove(mem, (char *)pa, PGSIZE);

    // 6. 在子进程的页表中建立映射：虚拟地址 i -> 新物理页 mem
    // 注意：权限位 flags 必须和父进程完全一致
    if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0)
    {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  // 如果中间出错了（比如内存不够），需要释放掉已经分配的页面
  // uvmunmap(new, 0, i, 1); // 这是一个假设你有的清理函数
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