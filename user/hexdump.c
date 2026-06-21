#include "proc.h"
#include "types.h"

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2
#define O_RDONLY 0
#define READ_CAP 16

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

static char hex_digit(uint value)
{
    value &= 0xf;
    return value < 10 ? '0' + value : 'a' + (value - 10);
}

static void write_hex32(uint64 value)
{
    char buf[8];

    for (int i = 7; i >= 0; i--)
    {
        buf[i] = hex_digit(value);
        value >>= 4;
    }
    write_bytes(STDOUT_FD, buf, sizeof(buf));
}

static int printable(uchar c)
{
    return c >= 32 && c < 127;
}

static void dump_line(uint64 offset, const uchar *buf, int n)
{
    char byte[3];

    write_hex32(offset);
    write_str(STDOUT_FD, "  ");

    for (int i = 0; i < READ_CAP; i++)
    {
        if (i < n)
        {
            byte[0] = hex_digit(buf[i] >> 4);
            byte[1] = hex_digit(buf[i]);
            byte[2] = ' ';
            write_bytes(STDOUT_FD, byte, sizeof(byte));
        }
        else
        {
            write_str(STDOUT_FD, "   ");
        }

        if (i == 7)
            write_str(STDOUT_FD, " ");
    }

    write_str(STDOUT_FD, " |");
    for (int i = 0; i < n; i++)
    {
        char c = printable(buf[i]) ? (char)buf[i] : '.';
        write_bytes(STDOUT_FD, &c, 1);
    }
    write_str(STDOUT_FD, "|\n");
}

static int dump_fd(int fd)
{
    uchar buf[READ_CAP];
    uint64 offset = 0;
    int n;

    while ((n = (int)syscall(SYS_read, fd, (uint64)buf, sizeof(buf))) > 0)
    {
        dump_line(offset, buf, n);
        offset += n;
    }

    return n < 0 ? -1 : 0;
}

static int dump_file(const char *path)
{
    int fd = (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);
    int result;

    if (fd < 0)
    {
        write_str(STDERR_FD, "hexdump: cannot open ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
        return -1;
    }

    result = dump_fd(fd);
    syscall(SYS_close, fd, 0, 0);
    if (result < 0)
    {
        write_str(STDERR_FD, "hexdump: read error ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
    }
    return result;
}

void main(int argc, char *argv[])
{
    if (argc > 2)
    {
        write_str(STDERR_FD, "usage: hexdump [FILE]\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    if (argc == 1)
        syscall(SYS_exit, dump_fd(STDIN_FD) < 0 ? -1 : 0, 0, 0);

    syscall(SYS_exit, dump_file(argv[1]) < 0 ? -1 : 0, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
