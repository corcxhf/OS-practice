#include "proc.h"
#include "types.h"
void put_str(const char *s);

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        put_str(argv[i]);
        if (i < argc - 1)
        {
            put_str(" ");
        }
    }
    put_str("\n");

    register uint64 a0_asm __asm__("a0") = 0; // status = 0
    register uint64 a7_asm __asm__("a7") = SYS_exit;
    __asm__ volatile("ecall" : : "r"(a0_asm), "r"(a7_asm) : "memory");
    return 0;
}

void put_str(const char *s)
{
    int len = 0;
    while (s[len])
        len++;

    register uint64 a0_asm __asm__("a0") = 1;
    register uint64 a1_asm __asm__("a1") = (uint64)s;
    register uint64 a2_asm __asm__("a2") = len;
    register uint64 a7_asm __asm__("a7") = SYS_write;
    __asm__ volatile("ecall" : : "r"(a0_asm), "r"(a1_asm), "r"(a2_asm), "r"(a7_asm) : "memory");
}