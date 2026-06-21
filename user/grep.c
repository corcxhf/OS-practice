#include "proc.h"
#include "types.h"

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2
#define O_RDONLY 0
#define LINE_CAP 1024
#define READ_CAP 256

struct matcher
{
    const char *pattern;
    int pattern_len;
    int (*match)(const struct matcher *, const char *);
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

static int brute_match(const struct matcher *m, const char *line)
{
    const char *pat = m->pattern;

    if (m->pattern_len == 0)
        return 1;

    for (int i = 0; line[i]; i++)
    {
        int j = 0;
        while (pat[j] && line[i + j] == pat[j])
            j++;
        if (!pat[j])
            return 1;
    }
    return 0;
}

static void matcher_init(struct matcher *m, const char *pattern)
{
    m->pattern = pattern;
    m->pattern_len = str_len(pattern);
    m->match = brute_match;
}

static void print_match(const char *name, int show_name, const char *line)
{
    if (show_name)
    {
        write_str(STDOUT_FD, name);
        write_str(STDOUT_FD, ":");
    }
    write_str(STDOUT_FD, line);
    write_str(STDOUT_FD, "\n");
}

static int flush_line(struct matcher *m, const char *name, int show_name, char *line, int *pos)
{
    int matched;

    line[*pos] = '\0';
    matched = m->match(m, line);
    if (matched)
        print_match(name, show_name, line);
    *pos = 0;
    return matched;
}

static int grep_fd(int fd, const char *name, int show_name, struct matcher *m)
{
    char read_buf[READ_CAP];
    char line[LINE_CAP];
    int pos = 0;
    int matched = 0;
    int n;

    while ((n = (int)syscall(SYS_read, fd, (uint64)read_buf, sizeof(read_buf))) > 0)
    {
        for (int i = 0; i < n; i++)
        {
            char c = read_buf[i];

            if (c == '\r')
                continue;
            if (c == '\n')
            {
                if (flush_line(m, name, show_name, line, &pos))
                    matched = 1;
                continue;
            }
            if (pos < LINE_CAP - 1)
                line[pos++] = c;
        }
    }

    if (pos > 0 && flush_line(m, name, show_name, line, &pos))
        matched = 1;
    return matched;
}

static int grep_file(const char *path, int show_name, struct matcher *m, int *had_error)
{
    int fd = (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);
    int matched;

    if (fd < 0)
    {
        write_str(STDERR_FD, "grep: cannot open ");
        write_str(STDERR_FD, path);
        write_str(STDERR_FD, "\n");
        *had_error = 1;
        return 0;
    }

    matched = grep_fd(fd, path, show_name, m);
    syscall(SYS_close, fd, 0, 0);
    return matched;
}

void main(int argc, char *argv[])
{
    struct matcher m;
    int matched = 0;
    int had_error = 0;

    if (argc < 2)
    {
        write_str(STDERR_FD, "usage: grep PATTERN [FILE...]\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    matcher_init(&m, argv[1]);

    if (argc == 2)
    {
        matched = grep_fd(STDIN_FD, "-", 0, &m);
    }
    else
    {
        int show_name = argc > 3;
        for (int i = 2; i < argc; i++)
        {
            if (grep_file(argv[i], show_name, &m, &had_error))
                matched = 1;
        }
    }

    if (had_error)
        syscall(SYS_exit, -1, 0, 0);
    syscall(SYS_exit, matched ? 0 : 1, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
