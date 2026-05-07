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

/* 系统调用号常量定义 */
#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_getpid 11
#define SYS_sbrk 12

#define SYS_open 15
#define SYS_write 16
#define SYS_read 17
#define SYS_close 18

/* 获取定义长度的宏 */
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
extern uint64 sys_getpid();
extern uint64 sys_exit(void);
// extern uint64 sys_fork(void);
extern uint64 sys_write(void);
extern uint64 sys_open(void);
extern uint64 sys_read(void);
extern uint64 sys_close(void);
extern uint64 walkaddr(pagetable_t pagetable, uint64 va);
// extern uint64 sys_wait(void);

/* ================================================================
 * TODO [Lab6-任务3-步骤1]：
 *   完善系统调用函数指针表 syscalls[]。
 *
 *   工作原理：
 *     syscalls[11] = sys_getpid
 *     当用户程序将 a7=11 并执行 ecall 时，
 *     syscall() 会调用 syscalls[11]()，即 sys_getpid()。
 *
 *   目前只实现 sys_getpid，其余留空（NULL）。
 *   后续可按需添加更多系统调用。
 * ================================================================ */
static uint64 (*syscalls[20])(void) = {
    [SYS_getpid] = sys_getpid,
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_write] = sys_write,
    [SYS_wait] = sys_wait,

    [SYS_open] sys_open,
    [SYS_read] sys_read,
    [SYS_close] sys_close,
};

/* ================================================================
 * syscall — 系统调用分发主函数（由 usertrap 调用）
 * ================================================================ */
void syscall(void)
{
    struct proc *p = myproc();

    /* 从陷阱帧读取系统调用号（用户在 a7 寄存器中填入的值）*/
    int num = p->trapframe->a7;
    if (1 <= num && num < NELEM(syscalls) && syscalls[num] != 0)
        p->trapframe->a0 = syscalls[num]();
    else
        p->trapframe->a0 = -1;

    /* ================================================================
     * TODO [Lab6-任务3-步骤2]：
     *   1. 检查 num 是否在合法范围内（1 <= num < NELEM(syscalls)），
     *      且 syscalls[num] 不为 NULL。
     *   2. 若合法，调用 syscalls[num]()，
     *      将返回值存入 p->trapframe->a0（用户程序会从 a0 读取返回值）。
     *   3. 若非法，打印错误并将 p->trapframe->a0 = -1（返回错误码）。
     * ================================================================ */
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
int fetchstr(uint64 addr, char *buf, int max)
{
    char *src = (char *)addr;
    uint64 s = r_sstatus();
    w_sstatus(s | SSTATUS_SUM);
    for (int i = 0; i < max - 1; i++)
    {
        if (addr < 0x10)
            return -1;

        buf[i] = src[i];
        if (src[i] == '\0')
            return i;
    }
    return -1;
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