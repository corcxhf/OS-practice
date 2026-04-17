#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "proc.h"
#include "types.h"

// 初始化锁

extern struct cpu *mycpu(void);

void initlock(struct spinlock *lk, char *name)
{
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

void push_off(void)
{
    int old = intr_get(); // 记录当前中断状态
    intr_off();           // 关中断

    struct cpu *c = mycpu();
    if (c->noff == 0)
        c->intena = old; // 第一次关中断时，记录原始的中断使能状态
    c->noff += 1;        // 嵌套计数
}

void pop_off(void)
{
    struct cpu *c = mycpu();
    c->noff -= 1;
    if (c->noff == 0 && c->intena)
        intr_on(); // 只有当嵌套计数归零，且最初是开着中断时，才重新开中断
}

void acquire(struct spinlock *lk)
{
    push_off();
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ; // 自旋等待

    __sync_synchronize(); // 内存屏障，确保之后的指令不会排到加锁前
    lk->cpu = mycpu();
}

void release(struct spinlock *lk)
{
    lk->cpu = 0;
    __sync_synchronize();
    __sync_lock_release(&lk->locked);
    pop_off();
}

int holding(struct spinlock *lk)
{
    int r;
    r = (lk->locked && lk->cpu == mycpu());
    return r;
}