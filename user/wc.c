#include "proc.h"
#include "types.h"

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2
#define O_RDONLY 0
#define READ_CAP 512

struct counts
{
    uint64 lines;
    uint64 words;
    uint64 bytes;
};

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

static int is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

static void count_buf(struct counts *c, const char *buf, int n, int *in_word)
{
    c->bytes += n;
    for (int i = 0; i < n; i++)
    {
        if (buf[i] == '\n')
            c->lines++;

        if (is_space(buf[i]))
        {
            *in_word = 0;
        }
        else if (!*in_word)
        {
            c->words++;
            *in_word = 1;
        }
    }
}

static int count_fd(int fd, struct counts *c)
{
    char buf[READ_CAP];
    int in_word = 0;
    int n;

    while ((n = (int)syscall(SYS_read, fd, (uint64)buf, sizeof(buf))) > 0)
        count_buf(c, buf, n, &in_word);

    return n < 0 ? -1 : 0;
}

static void add_counts(struct counts *dst, const struct counts *src)
{
    dst->lines += src->lines;
    dst->words += src->words;
    dst->bytes += src->bytes;
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

static void print_counts(const struct counts *c, const char *name)
{
    write_uint(c->lines);
    write_str(STDOUT_FD, " ");
    write_uint(c->words);
    write_str(STDOUT_FD, " ");
    write_uint(c->bytes);
    if (name)
    {
        write_str(STDOUT_FD, " ");
        write_str(STDOUT_FD, name);
    }
    write_str(STDOUT_FD, "\n");
}

static int count_file(const char *path, struct counts *c)
{
    int fd = (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);

    if (fd < 0)
    {
        write_str(STDERR_FD, "wc: cannot open ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
        return -1;
    }

    if (count_fd(fd, c) < 0)
    {
        write_str(STDERR_FD, "wc: read error ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
        syscall(SYS_close, fd, 0, 0);
        return -1;
    }

    syscall(SYS_close, fd, 0, 0);
    return 0;
}

void main(int argc, char *argv[])
{
    int had_error = 0;

    if (argc == 1)
    {
        struct counts c = {0, 0, 0};
        if (count_fd(STDIN_FD, &c) < 0)
            syscall(SYS_exit, -1, 0, 0);
        print_counts(&c, 0);
        syscall(SYS_exit, 0, 0, 0);
    }

    struct counts total = {0, 0, 0};
    for (int i = 1; i < argc; i++)
    {
        struct counts c = {0, 0, 0};
        if (count_file(argv[i], &c) < 0)
        {
            had_error = 1;
            continue;
        }
        print_counts(&c, argv[i]);
        add_counts(&total, &c);
    }

    if (argc > 2)
        print_counts(&total, "total");

    syscall(SYS_exit, had_error ? -1 : 0, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
