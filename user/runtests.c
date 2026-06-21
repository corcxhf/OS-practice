#include "proc.h"
#include "types.h"

#define STDOUT_FD 1
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 0x200
#define O_TRUNC 0x400
#define READ_CAP 1024

static char read_buf[READ_CAP];

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

static int str_contains(const char *haystack, const char *needle)
{
    if (!needle[0])
        return 1;

    for (int i = 0; haystack[i]; i++)
    {
        int j = 0;
        while (needle[j] && haystack[i + j] == needle[j])
            j++;
        if (!needle[j])
            return 1;
    }
    return 0;
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

static void write_uint(uint64 value)
{
    char buf[24];
    int pos = sizeof(buf);

    if (value == 0)
    {
        write_str("0");
        return;
    }

    while (value > 0 && pos > 0)
    {
        buf[--pos] = '0' + (value % 10);
        value /= 10;
    }
    write_bytes(&buf[pos], sizeof(buf) - pos);
}

static int open_file(const char *path, int flags)
{
    return (int)syscall(SYS_open, (uint64)path, flags, 0);
}

static void close_file(int fd)
{
    syscall(SYS_close, fd, 0, 0);
}

static void unlink_file(const char *path)
{
    syscall(SYS_unlink, (uint64)path, 0, 0);
}

static int write_file(const char *path, const char *data)
{
    int fd = open_file(path, O_WRONLY | O_CREAT | O_TRUNC);
    int len = str_len(data);
    int written = 0;

    if (fd < 0)
        return -1;

    while (written < len)
    {
        int n = (int)syscall(SYS_write, fd, (uint64)(data + written), len - written);
        if (n <= 0)
        {
            close_file(fd);
            return -1;
        }
        written += n;
    }
    close_file(fd);
    return 0;
}

static int read_file(const char *path, char *buf, int cap)
{
    int fd = open_file(path, O_RDONLY);
    int total = 0;
    int n;

    if (fd < 0 || cap <= 0)
        return -1;

    while (total < cap - 1 &&
           (n = (int)syscall(SYS_read, fd, (uint64)(buf + total), cap - 1 - total)) > 0)
    {
        total += n;
    }
    buf[total] = '\0';
    close_file(fd);
    return total;
}

static int file_contains(const char *path, const char *needle)
{
    if (read_file(path, read_buf, sizeof(read_buf)) < 0)
        return 0;
    return str_contains(read_buf, needle);
}

static int file_missing(const char *path)
{
    int fd = open_file(path, O_RDONLY);

    if (fd < 0)
        return 1;
    close_file(fd);
    return 0;
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
            fd = open_file(out_path, O_WRONLY | O_CREAT | O_TRUNC);
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

static int expect_status(char **argv, const char *out_path, int expected)
{
    return run_program(argv, out_path) == expected ? 0 : -1;
}

static void cleanup_files(void)
{
    unlink_file("rt_a");
    unlink_file("rt_b");
    unlink_file("rt_c");
    unlink_file("rt_lines");
    unlink_file("rt_mvsrc");
    unlink_file("rt_mvdst");
    unlink_file("rt_out");
    unlink_file("rt_libc");
    unlink_file("rt_fs");
    unlink_file("build.log");
    unlink_file("ct_trunc");
    unlink_file("ct_stdio");
    unlink_file("ct_seek");
    unlink_file("fs_basic");
    unlink_file("fs_trunc");
    unlink_file("fs_unlink");
    unlink_file("fs_seek");
    unlink_file("fs_offset");
    unlink_file("fsra");
    unlink_file("fsrb");
    unlink_file("fsrc");
    unlink_file("fsrd");
    unlink_file("fsre");
    unlink_file("fsrf");
    unlink_file("fsrg");
    unlink_file("fsrh");
    unlink_file("fsri");
    unlink_file("fsrj");
    unlink_file("fsrk");
    unlink_file("fsrl");
    unlink_file("fsrm");
    unlink_file("fsrn");
    unlink_file("fsro");
    unlink_file("fsrp");
}

static int prepare_base_files(void)
{
    if (write_file("rt_a", "alpha\nbeta\n") < 0)
        return -1;
    if (write_file("rt_c", "alpha\nzeta\n") < 0)
        return -1;
    if (write_file("rt_lines",
                   "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\nl11\nl12\n") < 0)
        return -1;
    return 0;
}

static int test_cp_cmp(void)
{
    char *cp_argv[] = {"/bin/cp", "rt_a", "rt_b", 0};
    char *cmp_argv[] = {"/bin/cmp", "rt_a", "rt_b", 0};

    if (expect_status(cp_argv, 0, 0) < 0)
        return -1;
    if (expect_status(cmp_argv, "rt_out", 0) < 0)
        return -1;
    return file_contains("rt_out", "differ") ? -1 : 0;
}

static int test_cmp_diff(void)
{
    char *argv[] = {"/bin/cmp", "rt_a", "rt_c", 0};

    if (expect_status(argv, "rt_out", 1) < 0)
        return -1;
    return file_contains("rt_out", "differ: byte") ? 0 : -1;
}

static int test_wc(void)
{
    char *argv[] = {"/bin/wc", "rt_a", 0};

    if (expect_status(argv, "rt_out", 0) < 0)
        return -1;
    return file_contains("rt_out", "2 2 11 rt_a") ? 0 : -1;
}

static int test_head(void)
{
    char *argv[] = {"/bin/head", "-n", "3", "rt_lines", 0};

    if (expect_status(argv, "rt_out", 0) < 0)
        return -1;
    if (!file_contains("rt_out", "l1\nl2\nl3\n"))
        return -1;
    return file_contains("rt_out", "l4") ? -1 : 0;
}

static int test_tail(void)
{
    char *argv[] = {"/bin/tail", "-n", "2", "rt_lines", 0};

    if (expect_status(argv, "rt_out", 0) < 0)
        return -1;
    if (!file_contains("rt_out", "l11\nl12\n"))
        return -1;
    return file_contains("rt_out", "l10") ? -1 : 0;
}

static int test_hexdump(void)
{
    char *argv[] = {"/bin/hexdump", "rt_a", 0};

    if (expect_status(argv, "rt_out", 0) < 0)
        return -1;
    if (!file_contains("rt_out", "61 6c 70 68 61"))
        return -1;
    return file_contains("rt_out", "|alpha.beta.|") ? 0 : -1;
}

static int test_mv(void)
{
    char *argv[] = {"/bin/mv", "rt_mvsrc", "rt_mvdst", 0};

    if (write_file("rt_mvsrc", "move\n") < 0)
        return -1;
    if (expect_status(argv, 0, 0) < 0)
        return -1;
    if (!file_missing("rt_mvsrc"))
        return -1;
    return file_contains("rt_mvdst", "move\n") ? 0 : -1;
}

static int compile_contract(const char *source, const char *output)
{
    const char *target;
    char *argv[] = {"/bin/build", 0, 0};

    (void)source;
    if (str_eq(output, "rt_libc"))
        target = "libc-contract";
    else
        target = "fs-contract";
    argv[1] = (char *)target;
    return expect_status(argv, "rt_out", 0);
}

static int run_contract(const char *program, const char *pass_marker)
{
    char *argv[] = {(char *)program, 0};

    if (expect_status(argv, "rt_out", 0) < 0)
        return -1;
    return file_contains("rt_out", pass_marker) ? 0 : -1;
}

static int test_libc_contract(void)
{
    if (compile_contract("/src/tests/libc_ct.c", "rt_libc") < 0)
        return -1;
    return run_contract("./rt_libc", "LIBC_CONTRACT_PASS");
}

static int test_fs_contract(void)
{
    if (compile_contract("/src/tests/fs_ct.c", "rt_fs") < 0)
        return -1;
    return run_contract("./rt_fs", "FS_CONTRACT_PASS");
}

struct test_case
{
    const char *name;
    const char *group;
    int (*fn)(void);
};

static struct test_case tests[] = {
    {"cp-cmp", "tools", test_cp_cmp},
    {"cmp-diff", "tools", test_cmp_diff},
    {"wc", "tools", test_wc},
    {"head", "tools", test_head},
    {"tail", "tools", test_tail},
    {"hexdump", "tools", test_hexdump},
    {"mv", "tools", test_mv},
    {"libc-contract", "contracts", test_libc_contract},
    {"fs-contract", "contracts", test_fs_contract},
};

static int run_one(const char *name, int (*fn)(void))
{
    int ok = fn() == 0;

    write_str(ok ? "PASS " : "FAIL ");
    write_str(name);
    write_str("\n");
    return ok;
}

static int should_run(const struct test_case *test, const char *selector)
{
    if (selector == 0 || str_eq(selector, "all"))
        return 1;
    return str_eq(selector, test->group) || str_eq(selector, test->name);
}

static int selector_known(const char *selector)
{
    if (selector == 0 || str_eq(selector, "all") ||
        str_eq(selector, "tools") || str_eq(selector, "contracts"))
        return 1;

    for (int i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++)
    {
        if (str_eq(selector, tests[i].name))
            return 1;
    }
    return 0;
}

static void print_usage(void)
{
    write_str("usage: runtests [all|tools|contracts|list|help|TEST]\n");
}

static void print_group(const char *group)
{
    write_str(group);
    write_str(":\n");
    for (int i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++)
    {
        if (!str_eq(tests[i].group, group))
            continue;
        write_str("  ");
        write_str(tests[i].name);
        write_str("\n");
    }
}

static void print_list(void)
{
    print_group("tools");
    print_group("contracts");
}

void main(int argc, char *argv[])
{
    int passed = 0;
    int failed = 0;
    int selected = 0;
    const char *selector = 0;

    if (argc > 2)
    {
        print_usage();
        syscall(SYS_exit, 1, 0, 0);
    }
    if (argc == 2)
        selector = argv[1];

    if (selector && str_eq(selector, "help"))
    {
        print_usage();
        write_str("groups: all tools contracts\n");
        write_str("use 'runtests list' to show test names\n");
        syscall(SYS_exit, 0, 0, 0);
    }

    if (selector && str_eq(selector, "list"))
    {
        print_list();
        syscall(SYS_exit, 0, 0, 0);
    }

    if (!selector_known(selector))
    {
        print_usage();
        write_str("unknown test: ");
        write_str(selector);
        write_str("\nRUNTESTS_FAIL\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    cleanup_files();
    if (prepare_base_files() < 0)
    {
        write_str("FAIL setup\nRUNTESTS_FAIL\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    for (int i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++)
    {
        if (!should_run(&tests[i], selector))
            continue;

        selected++;
        if (run_one(tests[i].name, tests[i].fn))
            passed++;
        else
            failed++;
    }

    write_str("SUMMARY ");
    write_uint(passed);
    write_str(" passed ");
    write_uint(failed);
    write_str(" failed\n");
    write_str(failed == 0 ? "RUNTESTS_PASS\n" : "RUNTESTS_FAIL\n");

    cleanup_files();
    syscall(SYS_exit, failed == 0 ? 0 : 1, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
