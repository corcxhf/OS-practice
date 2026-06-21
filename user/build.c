#include "proc.h"
#include "types.h"

#define STDOUT_FD 1
#define O_WRONLY 1
#define O_CREAT 0x200
#define O_TRUNC 0x400

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

static void write_bytes(const char *s, int n)
{
    syscall(SYS_write, STDOUT_FD, (uint64)s, (uint64)n);
}

static void write_str(const char *s)
{
    write_bytes(s, str_len(s));
}

static int open_output(const char *path)
{
    return (int)syscall(SYS_open, (uint64)path, O_WRONLY | O_CREAT | O_TRUNC, 0);
}

static void unlink_file(const char *path)
{
    syscall(SYS_unlink, (uint64)path, 0, 0);
}

static int wait_for_child(int *status)
{
    int child_status = -1;
    int pid = (int)syscall(SYS_wait, (uint64)&child_status, 0, 0);

    if (status)
        *status = child_status;
    return pid;
}

static int run_program(char **argv, const char *out_path)
{
    int status = -1;
    int pid = (int)syscall(SYS_fork, 0, 0, 0);

    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        if (out_path)
        {
            int fd;
            syscall(SYS_close, STDOUT_FD, 0, 0);
            fd = open_output(out_path);
            if (fd != STDOUT_FD)
                syscall(SYS_exit, -1, 0, 0);
        }

        syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);
        syscall(SYS_exit, -1, 0, 0);
    }

    if (wait_for_child(&status) < 0)
        return -1;
    return status;
}

static int build_target(const char *name, const char *source, const char *output)
{
    char *argv[] = {"/bin/tcc", (char *)source, "-o", (char *)output, 0};
    int status;

    unlink_file(output);
    status = run_program(argv, "build.log");
    if (status == 0)
    {
        write_str("BUILD_PASS ");
        write_str(name);
        write_str("\n");
        return 0;
    }

    write_str("BUILD_FAIL ");
    write_str(name);
    write_str("\n");
    return -1;
}

static int build_libc_contract(void)
{
    return build_target("libc-contract", "/src/tests/libc_ct.c", "rt_libc");
}

static int build_fs_contract(void)
{
    return build_target("fs-contract", "/src/tests/fs_ct.c", "rt_fs");
}

static int build_contracts(void)
{
    int failed = 0;

    if (build_libc_contract() < 0)
        failed++;
    if (build_fs_contract() < 0)
        failed++;
    return failed == 0 ? 0 : -1;
}

static void print_usage(void)
{
    write_str("usage: build [list|help|contracts|libc-contract|fs-contract]\n");
}

static void print_list(void)
{
    write_str("contracts:\n");
    write_str("  libc-contract\n");
    write_str("  fs-contract\n");
}

void main(int argc, char *argv[])
{
    int status = -1;

    if (argc != 2)
    {
        print_usage();
        syscall(SYS_exit, 1, 0, 0);
    }

    if (str_eq(argv[1], "help"))
    {
        print_usage();
        syscall(SYS_exit, 0, 0, 0);
    }
    if (str_eq(argv[1], "list"))
    {
        print_list();
        syscall(SYS_exit, 0, 0, 0);
    }
    if (str_eq(argv[1], "libc-contract"))
        status = build_libc_contract();
    else if (str_eq(argv[1], "fs-contract"))
        status = build_fs_contract();
    else if (str_eq(argv[1], "contracts"))
        status = build_contracts();
    else
    {
        print_usage();
        write_str("unknown target: ");
        write_str(argv[1]);
        write_str("\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    syscall(SYS_exit, status == 0 ? 0 : 1, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
