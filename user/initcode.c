// user/initcode.c
typedef unsigned long uint64;

#include "proc.h"

#define LINE_SIZE 128
#define MAX_ARGS 16
#define STDIN_FD 0
#define STDOUT_FD 1

static char input_buf[LINE_SIZE];
static char cwd_path[LINE_SIZE] = "/";

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

static int str_cmp(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

static int has_slash(const char *s)
{
    while (*s)
    {
        if (*s++ == '/')
            return 1;
    }
    return 0;
}

static void write_bytes(const char *s, int n)
{
    syscall(SYS_write, STDOUT_FD, (uint64)s, (uint64)n);
}

static void write_str(const char *s)
{
    write_bytes(s, str_len(s));
}

static void write_char(char c)
{
    syscall(SYS_write, STDOUT_FD, (uint64)&c, 1);
}

static void move_left(int count)
{
    static const char left[] = {27, '[', 'D'};
    while (count-- > 0)
        write_bytes(left, 3);
}

static void move_right(void)
{
    static const char right[] = {27, '[', 'C'};
    write_bytes(right, 3);
}

static void print_prompt(void)
{
    write_str("MyOS:");
    write_str(cwd_path);
    write_str("> ");
}

static void clear_args(char **argv)
{
    for (int i = 0; i < MAX_ARGS; i++)
        argv[i] = 0;
}

static int parse_args(char *line, char **argv)
{
    int argc = 0;
    int in_token = 0;

    clear_args(argv);
    for (int i = 0; line[i]; i++)
    {
        if (line[i] == ' ' || line[i] == '\t')
        {
            line[i] = '\0';
            in_token = 0;
        }
        else if (!in_token)
        {
            if (argc < MAX_ARGS - 1)
                argv[argc++] = &line[i];
            in_token = 1;
        }
    }
    argv[argc] = 0;
    return argc;
}

static int handle_arrow(int esc_state, char c, int *cursor, int len)
{
    if (esc_state == 1)
        return c == '[' ? 2 : 0;

    if (esc_state != 2)
        return 0;

    if (c == 'D' && *cursor > 0)
    {
        (*cursor)--;
        move_left(1);
    }
    else if (c == 'C' && *cursor < len)
    {
        (*cursor)++;
        move_right();
    }
    return 0;
}

static void erase_before_cursor(char *line, int *len, int *cursor)
{
    if (*cursor <= 0)
        return;

    int tail = *len - *cursor;
    for (int i = 0; i < tail; i++)
        line[*cursor - 1 + i] = line[*cursor + i];

    (*cursor)--;
    (*len)--;
    move_left(1);
    if (tail > 0)
        write_bytes(line + *cursor, tail);
    write_char(' ');
    move_left(tail + 1);
}

static void insert_at_cursor(char *line, int *len, int *cursor, char c)
{
    if (*len >= LINE_SIZE - 1)
        return;

    int tail = *len - *cursor;
    for (int i = tail - 1; i >= 0; i--)
        line[*cursor + 1 + i] = line[*cursor + i];

    line[*cursor] = c;
    (*len)++;
    write_bytes(line + *cursor, tail + 1);
    (*cursor)++;
    move_left(tail);
}

static int read_line(char *line)
{
    int len = 0;
    int cursor = 0;
    int esc_state = 0;

    for (;;)
    {
        char c;
        int n = syscall(SYS_read, STDIN_FD, (uint64)&c, 1);
        if (n <= 0)
            continue;

        if (c == 27)
        {
            esc_state = 1;
            continue;
        }
        if (esc_state)
        {
            esc_state = handle_arrow(esc_state, c, &cursor, len);
            continue;
        }

        if (c == '\n')
        {
            line[len] = '\0';
            write_char('\n');
            return len;
        }
        if (c == 127 || c == '\b')
        {
            erase_before_cursor(line, &len, &cursor);
            continue;
        }
        insert_at_cursor(line, &len, &cursor, c);
    }
}

static void update_cwd_display(const char *target)
{
    if (target[0] == '/')
    {
        int i = 0;
        while (target[i] && i < LINE_SIZE - 1)
        {
            cwd_path[i] = target[i];
            i++;
        }
        cwd_path[i] = '\0';
        return;
    }

    if (str_cmp(target, ".") == 0)
        return;

    if (str_cmp(target, "..") == 0)
    {
        int len = str_len(cwd_path);
        if (len <= 1)
            return;

        int i = len - 1;
        while (i > 0 && cwd_path[i] != '/')
            i--;
        if (i == 0)
            cwd_path[1] = '\0';
        else
            cwd_path[i] = '\0';
        return;
    }

    int len = str_len(cwd_path);
    if (len > 1 && cwd_path[len - 1] != '/')
        cwd_path[len++] = '/';
    for (int i = 0; target[i] && len < LINE_SIZE - 1; i++)
        cwd_path[len++] = target[i];
    cwd_path[len] = '\0';
}

static int run_builtin(int argc, char **argv)
{
    if (str_cmp(argv[0], "cd") == 0)
    {
        if (argc >= 2)
        {
            if ((int)syscall(SYS_chdir, (uint64)argv[1], 0, 0) == 0)
                update_cwd_display(argv[1]);
            else
                write_str("cd: cannot cd\n");
        }
        return 1;
    }

    if (str_cmp(argv[0], "pwd") == 0)
    {
        write_str(cwd_path);
        write_char('\n');
        return 1;
    }

    return 0;
}

static int join_path(char *dst, const char *prefix, const char *name)
{
    int n = 0;
    for (int i = 0; prefix[i] && n < LINE_SIZE - 1; i++)
        dst[n++] = prefix[i];
    for (int i = 0; name[i] && n < LINE_SIZE - 1; i++)
        dst[n++] = name[i];
    dst[n] = '\0';
    return n < LINE_SIZE - 1;
}

static void exec_command(char **argv)
{
    static const char *paths[] = {"/bin/", "/usr/bin/", "/", 0};
    char path[LINE_SIZE];

    if (argv[0] == 0)
        syscall(SYS_exit, -1, 0, 0);

    if (has_slash(argv[0]))
        syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);
    else
    {
        syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);
        for (int i = 0; paths[i]; i++)
        {
            if (join_path(path, paths[i], argv[0]))
                syscall(SYS_exec, (uint64)path, (uint64)argv, 0);
        }
    }

    write_str("Command not found!\n");
    syscall(SYS_exit, -1, 0, 0);
}

static int find_token(char **argv, const char *token)
{
    for (int i = 0; argv[i]; i++)
    {
        if (str_cmp(argv[i], token) == 0)
            return i;
    }
    return -1;
}

static void wait_for_child(void)
{
    int status;
    syscall(SYS_wait, (uint64)&status, 0, 0);
}

static void redirect_stdout_if_needed(char **argv)
{
    int redirect = find_token(argv, ">");
    if (redirect < 0)
        return;

    if (argv[redirect + 1] == 0)
    {
        write_str("syntax error: expected file after >\n");
        syscall(SYS_exit, -1, 0, 0);
    }

    syscall(SYS_close, STDOUT_FD, 0, 0);
    if ((int)syscall(SYS_open, (uint64)argv[redirect + 1], 0x201, 0) < 0)
        syscall(SYS_exit, -1, 0, 0);
    argv[redirect] = 0;
}

static void run_simple(char **argv)
{
    int pid = syscall(SYS_fork, 0, 0, 0);
    if (pid < 0)
    {
        write_str("Fork failed!\n");
        return;
    }
    if (pid == 0)
    {
        redirect_stdout_if_needed(argv);
        exec_command(argv);
    }
    wait_for_child();
}

static void run_pipeline(char **argv, int pipe_at)
{
    int fds[2];
    char **right_argv = &argv[pipe_at + 1];

    argv[pipe_at] = 0;
    if (argv[0] == 0 || right_argv[0] == 0)
    {
        write_str("syntax error: invalid pipe\n");
        return;
    }

    if (syscall(SYS_pipe, (uint64)fds, 0, 0) < 0)
    {
        write_str("Pipe creation failed!\n");
        return;
    }

    int left_pid = syscall(SYS_fork, 0, 0, 0);
    if (left_pid == 0)
    {
        syscall(SYS_close, STDOUT_FD, 0, 0);
        syscall(SYS_dup, fds[1], 0, 0);
        syscall(SYS_close, fds[0], 0, 0);
        syscall(SYS_close, fds[1], 0, 0);
        exec_command(argv);
    }

    int right_pid = syscall(SYS_fork, 0, 0, 0);
    if (right_pid == 0)
    {
        syscall(SYS_close, STDIN_FD, 0, 0);
        syscall(SYS_dup, fds[0], 0, 0);
        syscall(SYS_close, fds[1], 0, 0);
        syscall(SYS_close, fds[0], 0, 0);
        exec_command(right_argv);
    }

    syscall(SYS_close, fds[0], 0, 0);
    syscall(SYS_close, fds[1], 0, 0);
    wait_for_child();
    wait_for_child();
}

static void dispatch_command(int argc, char **argv)
{
    int pipe_at;

    if (argc == 0 || run_builtin(argc, argv))
        return;

    pipe_at = find_token(argv, "|");
    if (pipe_at >= 0)
        run_pipeline(argv, pipe_at);
    else
        run_simple(argv);
}

void main(void)
{
    char *argv[MAX_ARGS];

    for (;;)
    {
        int argc;

        print_prompt();
        if (read_line(input_buf) == 0)
            continue;

        argc = parse_args(input_buf, argv);
        dispatch_command(argc, argv);
    }
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
