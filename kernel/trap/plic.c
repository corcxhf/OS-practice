
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

// 假设你的 memlayout.h 已经定义了 PLIC 相关的宏和 UART0_IRQ (通常是 10)


void plicinithart(void)
{
    int hartid = r_tp(); // 获取当前核心编号


    *(uint32 *)PLIC_SENABLE(hartid) = (1 << UART0_IRQ);
    *(uint32 *)PLIC_SPRIORITY(hartid) = 0;
}