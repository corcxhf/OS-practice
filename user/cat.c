#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);

int str_len(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

void write_str(const char *s)
{
    syscall(SYS_write, 1, (uint64)s, (uint64)str_len(s));
}

void do_cat(int fd)
{
    char buf[512];
    int n;

    if (fd == 0)
    {
        while ((n = (int)syscall(SYS_read, 0, (uint64)buf, sizeof(buf))) > 0)
            syscall(SYS_write, 1, (uint64)buf, (uint64)n);
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
            write_str("cat: cannot open ");
            write_str(argv[i]);
            write_str("\n");
            continue;
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
