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

    // 带着 O_CREAT 标志发起请求！
    int fd = (int)syscall(SYS_open, (uint64)argv[1], O_CREAT | O_RDWR, 0);
    if (fd < 0)
    {
        char err[] = "touch: failed to create file\n";
        syscall(SYS_write, 1, (uint64)err, (uint64)str_len(err));
        syscall(SYS_exit, (uint64)-1, 0, 0);
    }

    // 建好了就直接关掉，留下一个空文件
    syscall(SYS_close, (uint64)fd, 0, 0);
    syscall(SYS_exit, 0, 0, 0);
}

int str_len(const char *s)
{
    int len = 0;
    while (s[len] != '\0')
        len++;
    return len;
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