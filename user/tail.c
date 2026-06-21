#include "proc.h"
#include "types.h"

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2
#define O_RDONLY 0
#define READ_CAP 256
#define TAIL_CAP 4096
#define DEFAULT_LINES 10

struct tailbuf
{
    char data[TAIL_CAP];
    int start;
    int len;
    int lines;
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

static int parse_uint(const char *s, int *out)
{
    int value = 0;

    if (!s[0])
        return -1;
    for (int i = 0; s[i]; i++)
    {
        if (s[i] < '0' || s[i] > '9')
            return -1;
        value = value * 10 + (s[i] - '0');
    }
    *out = value;
    return 0;
}

static char tail_get(const struct tailbuf *t, int offset)
{
    return t->data[(t->start + offset) % TAIL_CAP];
}

static void tail_drop_one(struct tailbuf *t)
{
    if (t->len <= 0)
        return;
    if (t->data[t->start] == '\n')
        t->lines--;
    t->start = (t->start + 1) % TAIL_CAP;
    t->len--;
}

static void tail_push(struct tailbuf *t, char c, int limit)
{
    int pos;

    if (t->len == TAIL_CAP)
        tail_drop_one(t);

    pos = (t->start + t->len) % TAIL_CAP;
    t->data[pos] = c;
    t->len++;
    if (c == '\n')
        t->lines++;

    while (limit >= 0 && t->lines > limit && t->len > 0)
        tail_drop_one(t);
}

static void tail_write(const struct tailbuf *t)
{
    int first;

    if (t->len <= 0)
        return;

    first = TAIL_CAP - t->start;
    if (first > t->len)
        first = t->len;
    write_bytes(STDOUT_FD, &t->data[t->start], first);
    if (first < t->len)
        write_bytes(STDOUT_FD, t->data, t->len - first);
}

static int tail_fd(int fd, int limit)
{
    struct tailbuf t;
    char buf[READ_CAP];
    int n;

    t.start = 0;
    t.len = 0;
    t.lines = 0;

    if (limit <= 0)
    {
        while ((n = (int)syscall(SYS_read, fd, (uint64)buf, sizeof(buf))) > 0)
            ;
        return n < 0 ? -1 : 0;
    }

    while ((n = (int)syscall(SYS_read, fd, (uint64)buf, sizeof(buf))) > 0)
    {
        for (int i = 0; i < n; i++)
            tail_push(&t, buf[i], limit);
    }

    if (n < 0)
        return -1;

    if (t.len > 0 && tail_get(&t, 0) == '\n')
        tail_drop_one(&t);
    tail_write(&t);
    return 0;
}

static int tail_file(const char *path, int limit)
{
    int fd = (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);
    int result;

    if (fd < 0)
    {
        write_str(STDERR_FD, "tail: cannot open ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
        return -1;
    }

    result = tail_fd(fd, limit);
    syscall(SYS_close, fd, 0, 0);
    if (result < 0)
    {
        write_str(STDERR_FD, "tail: read error ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
    }
    return result;
}

void main(int argc, char *argv[])
{
    int lines = DEFAULT_LINES;
    int arg = 1;

    if (argc >= 3 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0')
    {
        if (parse_uint(argv[2], &lines) < 0)
        {
            write_str(STDERR_FD, "usage: tail [-n N] [FILE]\n");
            syscall(SYS_exit, 1, 0, 0);
        }
        arg = 3;
    }

    if (argc - arg > 1)
    {
        write_str(STDERR_FD, "usage: tail [-n N] [FILE]\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    if (argc == arg)
    {
        syscall(SYS_exit, tail_fd(STDIN_FD, lines) < 0 ? -1 : 0, 0, 0);
    }

    syscall(SYS_exit, tail_file(argv[arg], lines) < 0 ? -1 : 0, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
