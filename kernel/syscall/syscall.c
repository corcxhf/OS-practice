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
extern uint64 sys_chdir(void);
extern uint64 sys_pipe(void);
extern uint64 sys_dup(void);
extern uint64 sys_unlink(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_lseek(void);
extern uint64 sys_ioctl(void);
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
    [SYS_chdir] = sys_chdir,
    [SYS_pipe] = sys_pipe,
    [SYS_dup] = sys_dup,
    [SYS_unlink] = sys_unlink,
    [SYS_sbrk] = sys_sbrk,
    [SYS_lseek] = sys_lseek,
    [SYS_ioctl] = sys_ioctl,
};

/* ================================================================
 * syscall — 系统调用分发主函数（由 usertrap 调用）
 * ================================================================ */

int syscallcnt = 0;

void syscall(void)
{
    struct proc *p = myproc();
    int num = p->trapframe->a7;

    // int is_target = 0;
    // // uint64 a0_val = p->trapframe->a0;
    // if (num == 16 && p->trapframe->a0 == 0)
    // {
    //     p->trapframe->a0 = 2;
    // }
    // if (num == SYS_open || num == SYS_lseek || num == SYS_close ||
    //     (num == SYS_write && a0_val != 1 && a0_val != 0))
    // {
    //     is_target = 1;
    //     printf("[SYSCALL TRACE] 发起 syscall %d, a0=%d, a1=0x%lx, a2=%d",
    //            num, (int)a0_val, p->trapframe->a1, p->trapframe->a2);
    // }

    if (1 <= num && num < NELEM(syscalls) && syscalls[num] != 0)
        p->trapframe->a0 = syscalls[num]();
    else
        p->trapframe->a0 = -1;
    // int ret = (int)p->trapframe->a0;

    // // 2. 打印追踪结果
    // if (is_target)
    // {
    //     printf("  => 返回值: %d\n", ret);
    // }
    // else if (ret == -1 && syscallcnt <= 40)
    // {
    //     // 🚨 3. 抓捕所有返回 -1 的漏网之鱼！
    //     printf("\n[BUG ALARM] 抓到真凶！syscall %d 返回了 -1! (调用时 a0=%d)\n", num, (int)a0_val);
    //     syscallcnt++;
    // }
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

int argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    // 1. 获取第 n 个参数（也就是用户传进来的 fd 整数值）
    if (argint(n, &fd) < 0)
        return -1;

    // 2. 越界与有效性检查
    // NOFILE 是进程最大打开文件数（通常定义在 param.h 里，比如 16）
    // myproc()->ofile[fd] 检查该位置是否真的分配了文件结构体
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;

    // 3. 安检通过，将结果填入指针并返回
    if (pfd)
        *pfd = fd;
    if (pf)
        *pf = f;

    return 0;
}

int fetchstr(uint64 s, char *dst, int max)
{
    struct proc *p = myproc();
    int i = 0;
    while (i < max)
    {
        uint64 va = s + i;
        uint64 page_va = PGROUNDDOWN(va);
        uint64 offset = va - page_va;
        pte_t *pte = walk(p->pagetable, page_va, 0);
        if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
            return -1;
        uint64 pa = PTE2PA(*pte);
        char *src_byte = (char *)(pa + offset);
        dst[i] = *src_byte;
        if (*src_byte == '\0')
            return i;
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
