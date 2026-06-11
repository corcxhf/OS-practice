#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);
int str_len(const char *s);

void main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        syscall(SYS_exit, 0, 0, 0);
    }

    // 1. 打开文件 (O_RDONLY = 0)
    int fd = (int)syscall(SYS_open, (uint64)argv[1], 0, 0);
    if (fd < 0)
    {
        char err[] = "cat: cannot open file\n";
        syscall(SYS_write, 1, (uint64)err, (uint64)str_len(err));
        syscall(SYS_exit, (uint64)-1, 0, 0);
    }

    // 2. 流式搬运：每次搬 512 字节，读到哪打到哪
    char buf[512];
    int n;
    while ((n = (int)syscall(SYS_read, (uint64)fd, (uint64)buf, sizeof(buf))) > 0)
    {
        syscall(SYS_write, 1, (uint64)buf, (uint64)n);
    }
    // char newline = '\n';
    // syscall(SYS_write, 1, (uint64)&newline, 1);

    syscall(SYS_close, (uint64)fd, 0, 0);
    syscall(SYS_exit, 0, 0, 0);
}

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm__("a0") = a0;
    register uint64 a1_asm __asm__("a1") = a1;
    register uint64 a2_asm __asm__("a2") = a2;
    register uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall"
                     : "+r"(a0_asm)
                     : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
                     : "memory");
    return a0_asm;
}

int str_len(const char *s)
{
    int len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}