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
    write_str(STDERR_FD, "usage: gcc [options] file...\n");
    syscall(SYS_exit, 1, 0, 0);
}

void main(int argc, char *argv[], char *envp[])
{
    char *cc_argv[MAX_TOOL_ARGS];
    int out_argc = 0;
    int user_argc = 0;

    if (append_arg(cc_argv, &out_argc, "/bin/cc") < 0)
        usage();

    for (int i = 1; i < argc; i++)
    {
        if (str_eq(argv[i], "--version"))
        {
            write_str(STDOUT_FD, "gcc: MyOS GCC driver slot, currently backed by /bin/cc\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(argv[i], "-print-prog-name=as"))
        {
            write_str(STDOUT_FD, "/bin/as\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        if (str_eq(argv[i], "-print-prog-name=ld"))
        {
            write_str(STDOUT_FD, "/bin/ld\n");
            syscall(SYS_exit, 0, 0, 0);
        }
        user_argc++;
        if (append_arg(cc_argv, &out_argc, argv[i]) < 0)
        {
            write_str(STDERR_FD, "gcc: too many arguments\n");
            syscall(SYS_exit, 1, 0, 0);
        }
    }

    if (user_argc == 0)
        usage();

    syscall(SYS_exec, (uint64)"/bin/cc", (uint64)cc_argv, (uint64)envp);
    write_str(STDERR_FD, "gcc: cannot exec /bin/cc\n");
    syscall(SYS_exit, -1, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
