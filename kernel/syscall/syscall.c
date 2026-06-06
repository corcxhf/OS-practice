/* syscall.c — 系统调用分发（Lab6 任务3）
 *
 * 当用户程序执行 ecall 并由 usertrap() 捕获后，
 * 调用本文件的 syscall() 函数进行分发：
 *   1. 从陷阱帧读取系统调用号（a7 寄存器的值）
 *   2. 在函数指针表中查找对应的内核实现函数
 *   3. 调用该函数，将返回值写回陷阱帧的 a0 寄存器
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"

/* 获取定义长度的宏 */
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
extern uint64 sys_getpid();
extern uint64 sys_exit(void);
// extern uint64 sys_fork(void);
extern uint64 sys_write(void);
extern uint64 sys_open(void);
extern uint64 sys_read(void);
extern uint64 sys_close(void);
extern uint64 sys_exec(void);
extern uint64 walkaddr(pagetable_t pagetable, uint64 va);
extern uint64 sys_fstat(void);
extern uint64 sys_mkdir(void);
// extern uint64 sys_wait(void);

static uint64 (*syscalls[30])(void) = {
    [SYS_getpid] = sys_getpid,
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_write] = sys_write,
    [SYS_wait] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_open] = sys_open,
    [SYS_read] = sys_read,
    [SYS_close] = sys_close,
    [SYS_fstat] = sys_fstat,
    [SYS_mkdir] = sys_mkdir,
};

/* ================================================================
 * syscall — 系统调用分发主函数（由 usertrap 调用）
 * ================================================================ */
void syscall(void)
{
    struct proc *p = myproc();
    int num = p->trapframe->a7;
    if (1 <= num && num < NELEM(syscalls) && syscalls[num] != 0)
        p->trapframe->a0 = syscalls[num]();
    else
        p->trapframe->a0 = -1;
}

static uint64 argraw(int n)
{
    struct proc *p = myproc();
    switch (n)
    {
    case 0:
        return p->trapframe->a0;
    case 1:
        return p->trapframe->a1;
    case 2:
        return p->trapframe->a2;
    case 3:
        return p->trapframe->a3;
    case 4:
        return p->trapframe->a4;
    case 5:
        return p->trapframe->a5;
    }
    panic("argraw");
    return -1;
}

int argint(int n, int *ip)
{
    *ip = argraw(n);
    return 0;
}

int argaddr(int n, uint64 *ap)
{
    *ap = argraw(n);
    return 0;
}

// 这是一个模拟 walkaddr 的逻辑，你需要根据你的页表实现来写
int fetchstr(uint64 s, char *dst, int max)
{
    struct proc *p = myproc();
    // 利用 walkaddr 顺着当前老进程的页表查出物理地址，然后安全搬运
    // 如果你没有 walkaddr，可以用你自己写的 walk(p->pagetable, s, 0) 算出物理地址 pa
    uint64 va = PGROUNDDOWN(s);
    uint64 offset = s - va;

    pte_t *pte = walk(p->pagetable, va, 0);
    if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
        return -1;

    uint64 pa = PTE2PA(*pte);
    char *src = (char *)(pa + offset);

    // 开始搬运字符串直到遇到 \0 或者满了
    int i = 0;
    while (i < max)
    {
        dst[i] = src[i];
        if (src[i] == '\0')
            return i; // 返回字符串长度
        i++;
    }
    return -1;
}

int fetchaddr(uint64 addr, uint64 *ip)
{
    struct proc *p = myproc();
    uint64 va = PGROUNDDOWN(addr);
    uint64 offset = addr - va;

    if (addr + sizeof(uint64) > p->sz)
        return -1;

    pte_t *pte = walk(p->pagetable, va, 0);
    if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
        return -1;

    uint64 pa = PTE2PA(*pte);
    // 直接从转换后的物理地址读取用户存在那里的指针值
    *ip = *(uint64 *)(pa + offset);
    return 0;
}

int strlen(const char *s)
{
    int n;
    for (n = 0; s[n]; n++)
        ;
    return n;
}

char *safestrcpy(char *s, const char *t, int n)
{
    char *os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0)
        ;
    *s = 0;
    return os;
}

int argstr(int n, char *buf, int max)
{
    uint64 addr;
    if (argaddr(n, &addr) < 0)
        return -1;
    return fetchstr(addr, buf, max);
}