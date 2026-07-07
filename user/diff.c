#include "proc.h"
#include "types.h"

#define STDOUT_FD 1
#define STDERR_FD 2
#define O_RDONLY 0
#define LINE_CAP 512

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

static void write_bytes(int fd, const char *s, int n)
{
    syscall(SYS_write, fd, (uint64)s, (uint64)n);
}

static void write_str(int fd, const char *s)
{
    write_bytes(fd, s, str_len(s));
}

static void write_uint(uint64 value)
{
    char buf[24];
    int pos = sizeof(buf);

    if (value == 0)
    {
        write_str(STDOUT_FD, "0");
        return;
    }

    while (value > 0 && pos > 0)
    {
        buf[--pos] = '0' + (value % 10);
        value /= 10;
    }
    write_bytes(STDOUT_FD, &buf[pos], sizeof(buf) - pos);
}

static int close_status(int fd)
{
    return (int)syscall(SYS_close, fd, 0, 0);
}

static int open_input(const char *path)
{
    int fd = (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);

    if (fd < 0)
    {
        write_str(STDERR_FD, "diff: cannot open ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
    }
    return fd;
}

static int read_line(int fd, char *buf, int cap, int *truncated)
{
    int pos = 0;
    char ch;

    *truncated = 0;
    for (;;)
    {
        int n = (int)syscall(SYS_read, fd, (uint64)&ch, 1);

        if (n < 0)
            return -1;
        if (n == 0)
        {
            if (pos == 0)
                return 0;
            buf[pos] = 0;
            return 1;
        }
        if (ch == '\r')
            continue;
        if (ch == '\n')
        {
            buf[pos] = 0;
            return 1;
        }
        if (pos < cap - 1)
            buf[pos++] = ch;
        else
            *truncated = 1;
    }
}

static void print_difference(const char *left, const char *right, uint64 line,
                             const char *left_line, const char *right_line)
{
    write_str(STDOUT_FD, left);
    write_str(STDOUT_FD, " ");
    write_str(STDOUT_FD, right);
    write_str(STDOUT_FD, " differ: line ");
    write_uint(line);
    write_str(STDOUT_FD, "\n");

    write_str(STDOUT_FD, "- ");
    write_str(STDOUT_FD, left_line ? left_line : "<EOF>");
    write_str(STDOUT_FD, "\n+ ");
    write_str(STDOUT_FD, right_line ? right_line : "<EOF>");
    write_str(STDOUT_FD, "\n");
}

static int diff_files(const char *left, const char *right)
{
    char lbuf[LINE_CAP];
    char rbuf[LINE_CAP];
    int lfd;
    int rfd;
    uint64 line = 1;

    lfd = open_input(left);
    if (lfd < 0)
        return -1;

    rfd = open_input(right);
    if (rfd < 0)
    {
        close_status(lfd);
        return -1;
    }

    for (;;)
    {
        int ltrunc;
        int rtrunc;
        int lr = read_line(lfd, lbuf, sizeof(lbuf), &ltrunc);
        int rr = read_line(rfd, rbuf, sizeof(rbuf), &rtrunc);

        if (lr < 0)
        {
            write_str(STDERR_FD, "diff: read error ");
            write_str(STDERR_FD, left);
            write_str(STDERR_FD, "\n");
            close_status(lfd);
            close_status(rfd);
            return -1;
        }
        if (rr < 0)
        {
            write_str(STDERR_FD, "diff: read error ");
            write_str(STDERR_FD, right);
            write_str(STDERR_FD, "\n");
            close_status(lfd);
            close_status(rfd);
            return -1;
        }
        if (lr == 0 && rr == 0)
            break;
        if (lr != rr || ltrunc != rtrunc || !str_eq(lbuf, rbuf))
        {
            print_difference(left, right, line, lr ? lbuf : 0, rr ? rbuf : 0);
            close_status(lfd);
            close_status(rfd);
            return 1;
        }
        line++;
    }

    close_status(lfd);
    close_status(rfd);
    return 0;
}

void main(int argc, char *argv[])
{
    int result;

    if (argc != 3)
    {
        write_str(STDERR_FD, "usage: diff FILE1 FILE2\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    result = diff_files(argv[1], argv[2]);
    syscall(SYS_exit, result, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
