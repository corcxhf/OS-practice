#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);
int str_len(const char *s);

void main(int argc, char *argv[])
{
    // ==========================================================
    // 🚨 终极魔法：ANSI 转义序列
    // \x1b 也就是 ASCII 码的 ESC (Escape)
    // [2J 清空屏幕
    // [H  将光标归位到 (0,0)
    // ==========================================================
    char magic[] = "\x1b[2J\x1b[H";

    // 直接把这串咒语写到标准输出 (fd = 1)
    syscall(SYS_write, 1, (uint64)magic, (uint64)str_len(magic));

    // 深藏功与名，优雅退出
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