#include "proc.h"
#include "types.h"

#define STDOUT_FD 1
#define STDERR_FD 2
#define MAX_TOOL_ARGS 64

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

static int str_len(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

static void write_str(int fd, const char *s)
{
    syscall(SYS_write, fd, (uint64)s, str_len(s));
}

static int append_arg(char **out, int *count, char *arg)
{
    if (*count >= MAX_TOOL_ARGS - 1)
        return -1;
    out[(*count)++] = arg;
    out[*count] = 0;
    return 0;
}

static void usage(void)
{
    write_str(STDERR_FD, "usage: cpp [options] file...\n");
    syscall(SYS_exit, 1, 0, 0);
}

void main(int argc, char *argv[], char *envp[])
{
    char *myos_argv[MAX_TOOL_ARGS];
    char *tcc_argv[MAX_TOOL_ARGS];
    int out_argc = 0;

    if (argc == 2 && str_eq(argv[1], "--version"))
    {
        write_str(STDOUT_FD, "cpp: MyOS C preprocessor slot\n");
        syscall(SYS_exit, 0, 0, 0);
    }

    if (append_arg(myos_argv, &out_argc, "/bin/myos-gcc") < 0 ||
        append_arg(myos_argv, &out_argc, "-E") < 0)
        usage();
    for (int i = 1; i < argc; i++)
        if (append_arg(myos_argv, &out_argc, argv[i]) < 0)
            usage();
    syscall(SYS_exec, (uint64)"/bin/myos-gcc", (uint64)myos_argv, (uint64)envp);

    out_argc = 0;
    if (append_arg(tcc_argv, &out_argc, "/bin/tcc") < 0 ||
        append_arg(tcc_argv, &out_argc, "-E") < 0)
        usage();
    for (int i = 1; i < argc; i++)
        if (append_arg(tcc_argv, &out_argc, argv[i]) < 0)
            usage();

    syscall(SYS_exec, (uint64)"/bin/tcc", (uint64)tcc_argv, (uint64)envp);
    write_str(STDERR_FD, "cpp: cannot exec /bin/tcc\n");
    syscall(SYS_exit, -1, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
