#include "types.h"
#include "proc.h"
// 请确保在你的某个头文件（如 syscall.h 里面定义了 SYS_unlink）

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);
int str_len(const char *s);

void main(int argc, char *argv[])
{
    // 1. 如果没带参数，提示用法
    if (argc < 2)
    {
        char usage[] = "Usage: rm files...\n";
        syscall(SYS_write, 1, (uint64)usage, str_len(usage));
        syscall(SYS_exit, 1, 0, 0);
    }

    // 2. 遍历参数，挨个爆破
    for (int i = 1; i < argc; i++)
    {
        // 调用内核的 unlink 系统调用
        if ((int)syscall(SYS_unlink, (uint64)argv[i], 0, 0) < 0)
        {
            // 如果删除失败（比如文件不存在，或者是个目录）
            char err1[] = "rm: failed to delete ";
            char err2[] = "\n";
            syscall(SYS_write, 1, (uint64)err1, str_len(err1));
            syscall(SYS_write, 1, (uint64)argv[i], str_len(argv[i]));
            syscall(SYS_write, 1, (uint64)err2, str_len(err2));
        }
    }

    // 3. 功成身退
    syscall(SYS_exit, 0, 0, 0);
}

// ---------------------------------------------------------
// 底层工具函数与入口定义
// ---------------------------------------------------------

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

void __attribute__((naked, section(".text.entry"))) _start()
{
    __asm__ volatile("call main");
}