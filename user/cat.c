#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);

void do_cat(int fd)
{
    char buf[512];
    int n;

    if (fd == 0)
    {
        int i = 0;
        char c;
        int is_pipe = 0; // 管道探测标记

        while (syscall(SYS_read, 0, (uint64)&c, 1) > 0)
        {
            // 🚨 1. 纯用户态 Ctrl+C 拦截：随时服毒自尽
            if (c == 3)
            {
                syscall(SYS_exit, 0, 0, 0);
            }

            // 🚨 2. 核心魔法：实时回显与管道探测的分水岭
            // 如果是键盘输入，在回车触发前，内核缓冲区里其实是一股脑堆积了你敲的所有字
            // 如果一上来 i == 0 读到的第一个字符不是回车，且我们把它写回屏幕
            if (i == 0 && c != '\n' && is_pipe == 0)
            {
                // 探测：我们在用户态直接把它吐出来！
                // 这样无论内核什么时候唤醒它，只要拿到字符，立刻在屏幕上补上回显！
                syscall(SYS_write, 1, (uint64)&c, 1);
            }
            else if (is_pipe == 0 && i > 0)
            {
                // 键盘盲打流的后续字符：由 cat 在用户态接管实时回显
                syscall(SYS_write, 1, (uint64)&c, 1);
            }
            else if (i == 0 && c != '\n' && is_pipe == 0)
            {
                // 判定为非键盘流入（无缝块数据）
                is_pipe = 1;
            }

            buf[i++] = c;

            // 遇到回车，或者缓冲区满了
            if (c == '\n' || i == 512)
            {
                // 如果是管道，我们只做静默单次打印
                if (is_pipe)
                {
                    syscall(SYS_write, 1, (uint64)buf, (uint64)i);
                    syscall(SYS_exit, 0, 0, 0); // 瞬间断尾，交回控制权
                }
                else
                {
                    // 如果是键盘输入，回车按下后，打印出 cat 标志性的“复读”行
                    syscall(SYS_write, 1, (uint64)buf, (uint64)i);
                    i = 0; // 清空，等待下一行
                }
            }
        }
    }
    else
    {
        // 普通文件流式读取
        while ((n = (int)syscall(SYS_read, (uint64)fd, (uint64)buf, sizeof(buf))) > 0)
        {
            syscall(SYS_write, 1, (uint64)buf, (uint64)n);
        }
    }
}

void main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        do_cat(0);
        syscall(SYS_exit, 0, 0, 0);
    }

    for (int i = 1; i < argc; i++)
    {
        int fd = (int)syscall(SYS_open, (uint64)argv[i], 0, 0);
        if (fd < 0)
        {
            syscall(SYS_exit, (uint64)-1, 0, 0);
        }
        do_cat(fd);
        syscall(SYS_close, (uint64)fd, 0, 0);
    }
    syscall(SYS_exit, 0, 0, 0);
}

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm__("a0") = a0;
    register uint64 a1_asm __asm__("a1") = a1;
    register uint64 a2_asm __asm__("a2") = a2;
    register uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(a0_asm) : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm) : "memory");
    return a0_asm;
}

void __attribute__((naked, section(".text.entry"))) _start() { __asm__ volatile("call main"); }