#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);
int str_len(const char *s);
void main(int argc, char *argv[])
{
    // 1. 参数校验：如果没有输入目录名，直接退出
    if (argc <= 1)
    {
        syscall(SYS_exit, 0, 0, 0);
    }

    // 2. 召唤造物主：调用 SYS_mkdir
    // 严格填满 4 个参数：系统调用号, 路径名, 填充0, 填充0
    if ((int)syscall(SYS_mkdir, (uint64)argv[1], 0, 0) < 0)
    {
        char err[] = "mkdir: failed to create directory\n";
        syscall(SYS_write, 1, (uint64)err, (uint64)str_len(err));
        // 报错退出
        syscall(SYS_exit, (uint64)-1, 0, 0);
    }

    // 3. 成功退出
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