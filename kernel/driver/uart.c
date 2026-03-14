/* uart.c — UART 串口驱动（Lab1 任务3）
 *
 * UART（Universal Asynchronous Receiver-Transmitter）是 QEMU 模拟的串口设备。
 * 通过"内存映射I/O (MMIO)"机制，向固定物理地址写数据 = 向终端输出字符。
 *
 * QEMU virt 平台的 UART0 基地址：0x10000000
 * 此地址已在 memlayout.h 中定义为 UART0。
 */

/* ================================================================
 * TODO [Lab1-任务3-拓展]：
 *   如果你想让 uart_putc 更稳定（等待UART真正准备好再发送），
 *   可以在写数据前检查"线状态寄存器 LSR"的第5位（发送保持寄存器空为1）。
 *   现在我们先用最简单的版本，直接写入即可正常工作。
 * ================================================================ */

/* 通过指针操作访问 UART 的 MMIO 寄存器
 * volatile 关键字告诉编译器：这个内存访问有"副作用"，不能被优化掉！
 *
 * 思考：如果去掉 volatile，用高优化级别(-O2)编译时，编译器可能认为
 * "你反复写同一地址的内存，最后一次写就够了"，会把中间的写操作删除，
 * 从而导致只有最后一个字符被发送！
 */
#define UART0_BASE 0x10000000L
#define Reg(offset) ((volatile unsigned char *)(UART0_BASE + (offset)))

void uart_putc(char c)
{
  *Reg(0) = c;
  (void)c;
}

void uart_puts(char *s)
{
  while (*s)
  {
    uart_putc(*s);
    s++;
  }
  (void)s;
}