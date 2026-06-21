#include "proc.h"
#include "types.h"

#define STDOUT_FD 1
#define STDERR_FD 2
#define O_RDONLY 0
#define CMP_CAP 512

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
        write_str(STDERR_FD, "cmp: cannot open ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
    }
    return fd;
}

static void print_difference(const char *left, const char *right, uint64 byte)
{
    write_str(STDOUT_FD, left);
    write_str(STDOUT_FD, " ");
    write_str(STDOUT_FD, right);
    write_str(STDOUT_FD, " differ: byte ");
    write_uint(byte);
    write_str(STDOUT_FD, "\n");
}

static int cmp_files(const char *left, const char *right)
{
    char lbuf[CMP_CAP];
    char rbuf[CMP_CAP];
    int lfd;
    int rfd;
    uint64 byte = 1;

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
        int ln = (int)syscall(SYS_read, lfd, (uint64)lbuf, sizeof(lbuf));
        int rn = (int)syscall(SYS_read, rfd, (uint64)rbuf, sizeof(rbuf));
        int common;

        if (ln < 0)
        {
            write_str(STDERR_FD, "cmp: read error ");
            write_str(STDERR_FD, left);
            write_str(STDERR_FD, "\n");
            close_status(lfd);
            close_status(rfd);
            return -1;
        }
        if (rn < 0)
        {
            write_str(STDERR_FD, "cmp: read error ");
            write_str(STDERR_FD, right);
            write_str(STDERR_FD, "\n");
            close_status(lfd);
            close_status(rfd);
            return -1;
        }

        common = ln < rn ? ln : rn;
        for (int i = 0; i < common; i++)
        {
            if (lbuf[i] != rbuf[i])
            {
                print_difference(left, right, byte + i);
                close_status(lfd);
                close_status(rfd);
                return 1;
            }
        }

        if (ln != rn)
        {
            print_difference(left, right, byte + common);
            close_status(lfd);
            close_status(rfd);
            return 1;
        }
        if (ln == 0)
            break;

        byte += ln;
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
        write_str(STDERR_FD, "usage: cmp FILE1 FILE2\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    result = cmp_files(argv[1], argv[2]);
    syscall(SYS_exit, result, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
