// user/glue.c
#include "types.h"
#include "stat.h"

#include "proc.h"

// 祖传无敌系统调用内联汇编
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm__("a0") = a0;
    register uint64 a1_asm __asm__("a1") = a1;
    register uint64 a2_asm __asm__("a2") = a2;
    register uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(a0_asm) : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm) : "memory");
    return a0_asm;
}

// 1. 桥接 exit
void exit(int status)
{
    syscall(SYS_exit, (uint64)status, 0, 0);
    while (1)
        ; // 绝对防御：万一内核没死掉，死循环拦截
}

// 2. 桥接 open
int open(const char *filename, int flags)
{
    // 这里的 flags TCC 传进来的是标准 POSIX 的 O_RDONLY 等
    // 确保你的内核 SYS_open 认得这些 flags（通常 0 是只读，1 是只写，2 是读写）
    return (int)syscall(SYS_open, (uint64)filename, (uint64)flags, 0);
}

// 3. 桥接 read
int read(int fd, void *buf, unsigned int n)
{
    return (int)syscall(SYS_read, (uint64)fd, (uint64)buf, (uint64)n);
}

// 4. 桥接 write
int write(int fd, const void *buf, unsigned int n)
{
    return (int)syscall(SYS_write, (uint64)fd, (uint64)buf, (uint64)n);
}

// 5. 桥接 close
int close(int fd)
{
    return (int)syscall(SYS_close, (uint64)fd, 0, 0);
}

// 6. 桥接 fstat
int fstat(int fd, struct stat *st)
{
    return (int)syscall(SYS_fstat, (uint64)fd, (uint64)st, 0);
}

// 7. 补全 TCC 偶尔会用到的极简内存搬运工具
void *memset(void *dst, int c, uint n)
{
    char *cdst = (char *)dst;
    for (uint i = 0; i < n; i++)
    {
        cdst[i] = c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, uint n)
{
    char *cdst = (char *)dst;
    const char *csrc = (const char *)src;
    for (uint i = 0; i < n; i++)
    {
        cdst[i] = csrc[i];
    }
    return dst;
}