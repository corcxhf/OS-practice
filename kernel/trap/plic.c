
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

// 假设你的 memlayout.h 已经定义了 PLIC 相关的宏和 UART0_IRQ (通常是 10)

void plicinit(void)
{
    *(uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;
    *(uint32 *)(PLIC + UART0_IRQ * 4) = 1;
    *(uint8 *)(UART0 + 1) = 1;
}

void plicinithart(void)
{
    int hartid = r_tp(); // 获取当前核心编号
    *(uint32 *)PLIC_SENABLE(hartid) = (1 << UART0_IRQ);
    *(uint32 *)PLIC_SENABLE(hartid) |= (1 << VIRTIO0_IRQ);
    *(uint32 *)PLIC_SPRIORITY(hartid) = 0;
}