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
        while ((n = (int)syscall(SYS_read, 0, (uint64)buf, sizeof(buf))) > 0)
        {
            for (int i = 0; i < n; i++)
            {
                if (buf[i] == 3)
                    syscall(SYS_exit, 0, 0, 0); // 瞬间断尾，交回控制权
            }
            syscall(SYS_write, 1, (uint64)buf, (uint64)n);
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
