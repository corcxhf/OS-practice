#ifndef MYOS_CXX_SYS_H
#define MYOS_CXX_SYS_H

using myos_uint64 = unsigned long;

#define MYOS_SYS_exit 2
#define MYOS_SYS_sbrk 12
#define MYOS_SYS_write 16

static inline myos_uint64 myos_syscall3(myos_uint64 n, myos_uint64 a0, myos_uint64 a1, myos_uint64 a2)
{
    register myos_uint64 a0_asm __asm__("a0") = a0;
    register myos_uint64 a1_asm __asm__("a1") = a1;
    register myos_uint64 a2_asm __asm__("a2") = a2;
    register myos_uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall"
                     : "+r"(a0_asm)
                     : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
                     : "memory");
    return a0_asm;
}

static inline void myos_exit(int status)
{
    myos_syscall3(MYOS_SYS_exit, (myos_uint64)status, 0, 0);
    for (;;)
    {
    }
}

static inline int myos_strlen(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

static inline void myos_write_str(const char *s)
{
    myos_syscall3(MYOS_SYS_write, 1, (myos_uint64)s, (myos_uint64)myos_strlen(s));
}

#endif
