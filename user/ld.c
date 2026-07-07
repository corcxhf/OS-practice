#include "proc.h"
#include "types.h"

#define STDERR_FD 2
#define MAX_TOOL_ARGS 32

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

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix)
    {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
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

static int option_takes_value(const char *arg)
{
    return str_eq(arg, "-e") ||
           str_eq(arg, "-m") ||
           str_eq(arg, "-T") ||
           str_eq(arg, "-Map") ||
           str_eq(arg, "--oformat") ||
           str_eq(arg, "--dynamic-linker");
}

static int skip_ld_flag(const char *arg)
{
    return str_eq(arg, "-static") ||
           str_eq(arg, "-nostdlib") ||
           str_eq(arg, "--gc-sections") ||
           str_eq(arg, "--eh-frame-hdr") ||
           str_eq(arg, "--as-needed") ||
           str_eq(arg, "--no-as-needed") ||
           str_eq(arg, "--start-group") ||
           str_eq(arg, "--end-group") ||
           str_eq(arg, "-s") ||
           starts_with(arg, "--build-id") ||
           starts_with(arg, "--hash-style") ||
           starts_with(arg, "-melf");
}

static void usage(void)
{
    write_str(STDERR_FD, "usage: ld [-r] [-o output] file.o ...\n");
    syscall(SYS_exit, 1, 0, 0);
}

void main(int argc, char *argv[], char *envp[])
{
    char *tcc_argv[MAX_TOOL_ARGS];
    char *output = 0;
    int out_argc = 0;
    int input_count = 0;

    append_arg(tcc_argv, &out_argc, "/bin/tcc");

    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (str_eq(arg, "-o"))
        {
            if (i + 1 >= argc)
                usage();
            output = argv[++i];
            continue;
        }
        if (starts_with(arg, "-o") && arg[2])
        {
            output = arg + 2;
            continue;
        }
        if (str_eq(arg, "-r"))
        {
            if (append_arg(tcc_argv, &out_argc, arg) < 0)
                usage();
            continue;
        }
        if (str_eq(arg, "-L") || str_eq(arg, "-l"))
        {
            if (i + 1 >= argc || append_arg(tcc_argv, &out_argc, arg) < 0 ||
                append_arg(tcc_argv, &out_argc, argv[++i]) < 0)
                usage();
            continue;
        }
        if (starts_with(arg, "-L") || starts_with(arg, "-l"))
        {
            if (append_arg(tcc_argv, &out_argc, arg) < 0)
                usage();
            continue;
        }
        if (option_takes_value(arg))
        {
            if (i + 1 >= argc)
                usage();
            i++;
            continue;
        }
        if (arg[0] == '-')
        {
            if (skip_ld_flag(arg))
                continue;
            write_str(STDERR_FD, "ld: unsupported option ");
            write_str(STDERR_FD, arg);
            write_str(STDERR_FD, "\n");
            syscall(SYS_exit, 1, 0, 0);
        }

        if (append_arg(tcc_argv, &out_argc, arg) < 0)
            usage();
        input_count++;
    }

    if (input_count == 0)
        usage();
    if (output)
    {
        if (append_arg(tcc_argv, &out_argc, "-o") < 0 ||
            append_arg(tcc_argv, &out_argc, output) < 0)
            usage();
    }

    syscall(SYS_exec, (uint64)"/bin/tcc", (uint64)tcc_argv, (uint64)envp);
    write_str(STDERR_FD, "ld: cannot exec /bin/tcc\n");
    syscall(SYS_exit, -1, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
