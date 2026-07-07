// user/initcode.c
typedef unsigned long uint64;

#include "proc.h"

#define LINE_SIZE 256
#define PATH_SIZE 128
#define MAX_ARGS 32
#define MAX_STAGES MAX_ARGS
#define TOKEN_BUF_SIZE (LINE_SIZE * 2)
#define STDIN_FD 0
#define STDOUT_FD 1
#define DEFAULT_PATH "/bin:/"
#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x200
#define O_TRUNC 0x400
#define SEEK_END 2

static char input_buf[LINE_SIZE];
static char token_buf[TOKEN_BUF_SIZE];
static char cwd_path[LINE_SIZE] = "/";
static char path_value[PATH_SIZE] = DEFAULT_PATH;
static char env_path[PATH_SIZE + 5] = "PATH=" DEFAULT_PATH;
static char *shell_env[] = {env_path, 0};
static char status_arg[16] = "0";
static int last_status = 0;

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

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix)
    {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

static int is_operator(char c)
{
    return c == '<' || c == '>' || c == '|';
}

static void copy_str(char *dst, int dst_size, const char *src)
{
    int i = 0;
    while (src[i] && i < dst_size - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
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

static int is_path_assignment(const char *arg)
{
    return starts_with(arg, "PATH=");
}

static void set_path_value(const char *value)
{
    char next[PATH_SIZE];
    int n = 0;

    for (int i = 0; value[i] && n < PATH_SIZE - 1;)
    {
        if (starts_with(&value[i], "$PATH"))
        {
            for (int j = 0; path_value[j] && n < PATH_SIZE - 1; j++)
                next[n++] = path_value[j];
            i += 5;
        }
        else
        {
            next[n++] = value[i++];
        }
    }
    next[n] = '\0';
    copy_str(path_value, PATH_SIZE, next);
    copy_str(env_path, sizeof(env_path), "PATH=");
    copy_str(env_path + 5, sizeof(env_path) - 5, path_value);
}

static void print_path(void)
{
    write_str("PATH=");
    write_str(path_value);
    write_char('\n');
}

static void status_to_str(int status)
{
    char digits[12];
    unsigned int value;
    int n = 0;
    int d = 0;

    if (status < 0)
    {
        status_arg[n++] = '-';
        value = (unsigned int)(-status);
    }
    else
    {
        value = (unsigned int)status;
    }

    do
    {
        digits[d++] = '0' + value % 10;
        value /= 10;
    } while (value && d < (int)sizeof(digits));

    while (d > 0 && n < (int)sizeof(status_arg) - 1)
        status_arg[n++] = digits[--d];
    status_arg[n] = '\0';
}

static void expand_args(int argc, char **argv)
{
    for (int i = 0; i < argc; i++)
    {
        if (str_cmp(argv[i], "$PATH") == 0)
            argv[i] = path_value;
        else if (str_cmp(argv[i], "$?") == 0)
        {
            status_to_str(last_status);
            argv[i] = status_arg;
        }
    }
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
    int out = 0;

    clear_args(argv);
    for (int i = 0; line[i] && argc < MAX_ARGS - 1 && out < TOKEN_BUF_SIZE - 1;)
    {
        while (is_space(line[i]))
            i++;
        if (!line[i])
            break;

        argv[argc++] = &token_buf[out];
        if (is_operator(line[i]))
        {
            token_buf[out++] = line[i++];
            if (token_buf[out - 1] == '>' && line[i] == '>' && out < TOKEN_BUF_SIZE - 1)
                token_buf[out++] = line[i++];
        }
        else
        {
            while (line[i] && !is_space(line[i]) && !is_operator(line[i]) && out < TOKEN_BUF_SIZE - 1)
                token_buf[out++] = line[i++];
        }
        token_buf[out++] = '\0';
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

static int run_builtin(int argc, char **argv, int *status)
{
    *status = 0;

    if (is_path_assignment(argv[0]))
    {
        if (argc == 1)
            set_path_value(argv[0] + 5);
        else
        {
            write_str("PATH: use PATH=value\n");
            *status = 1;
        }
        return 1;
    }

    if (str_cmp(argv[0], "export") == 0)
    {
        if (argc == 1)
            print_path();
        else if (argc == 2 && is_path_assignment(argv[1]))
            set_path_value(argv[1] + 5);
        else
        {
            write_str("export: only PATH is supported\n");
            *status = 1;
        }
        return 1;
    }

    if (str_cmp(argv[0], "PATH") == 0)
    {
        print_path();
        return 1;
    }

    if (str_cmp(argv[0], "cd") == 0)
    {
        if (argc >= 2)
        {
            if ((int)syscall(SYS_chdir, (uint64)argv[1], 0, 0) == 0)
                update_cwd_display(argv[1]);
            else
            {
                write_str("cd: cannot cd\n");
                *status = 1;
            }
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

static int build_path(char *dst, int dst_size, const char *dir, int dir_len, const char *name)
{
    int n = 0;

    if (dir_len == 0 || (dir_len == 1 && dir[0] == '.'))
    {
        for (int i = 0; name[i] && n < dst_size - 1; i++)
            dst[n++] = name[i];
        dst[n] = '\0';
        return name[0] == '\0' || n < dst_size - 1;
    }

    for (int i = 0; i < dir_len && n < dst_size - 1; i++)
        dst[n++] = dir[i];
    if (n > 0 && dst[n - 1] != '/' && n < dst_size - 1)
        dst[n++] = '/';
    for (int i = 0; name[i] && n < dst_size - 1; i++)
        dst[n++] = name[i];
    dst[n] = '\0';
    return n < dst_size - 1;
}

static void exec_from_path(char **argv)
{
    char path[LINE_SIZE];
    int start = 0;

    for (int i = 0;; i++)
    {
        if (path_value[i] == ':' || path_value[i] == '\0')
        {
            int len = i - start;
            if (build_path(path, sizeof(path), &path_value[start], len, argv[0]))
                syscall(SYS_exec, (uint64)path, (uint64)argv, (uint64)shell_env);
            if (path_value[i] == '\0')
                break;
            start = i + 1;
        }
    }
}

static void exec_command(char **argv)
{
    if (argv[0] == 0)
        syscall(SYS_exit, -1, 0, 0);

    if (has_slash(argv[0]))
        syscall(SYS_exec, (uint64)argv[0], (uint64)argv, (uint64)shell_env);
    else
        exec_from_path(argv);

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

static int is_redirect_token(const char *arg)
{
    return str_cmp(arg, "<") == 0 || str_cmp(arg, ">") == 0 || str_cmp(arg, ">>") == 0;
}

static int is_control_token(const char *arg)
{
    return is_redirect_token(arg) || str_cmp(arg, "|") == 0;
}

static void print_redirection_syntax_error(const char *token)
{
    write_str("syntax error: expected file after ");
    write_str(token);
    write_char('\n');
}

static int wait_for_child(int *status)
{
    int child_status = -1;
    int pid = syscall(SYS_wait, (uint64)&child_status, 0, 0);
    if (status)
        *status = child_status;
    return pid;
}

static void remove_arg_pair(char **argv, int at)
{
    int i = at;
    while (argv[i + 2])
    {
        argv[i] = argv[i + 2];
        i++;
    }
    argv[i] = 0;
    argv[i + 1] = 0;
}

static int redirect_fd(int target_fd, const char *path, int flags, int append)
{
    int fd;

    syscall(SYS_close, target_fd, 0, 0);
    fd = syscall(SYS_open, (uint64)path, flags, 0);
    if (fd < 0)
        return -1;
    if (append && (int)syscall(SYS_lseek, target_fd, 0, SEEK_END) < 0)
        return -1;
    return 0;
}

static int validate_redirections(char **argv)
{
    for (int i = 0; argv[i]; i++)
    {
        if (!is_redirect_token(argv[i]))
            continue;
        if (argv[i + 1] == 0 || is_control_token(argv[i + 1]))
        {
            print_redirection_syntax_error(argv[i]);
            return -1;
        }
    }
    return 0;
}

static int apply_redirections(char **argv)
{
    for (int i = 0; argv[i];)
    {
        int input = str_cmp(argv[i], "<") == 0;
        int output = str_cmp(argv[i], ">") == 0;
        int append = str_cmp(argv[i], ">>") == 0;

        if (!input && !output && !append)
        {
            i++;
            continue;
        }

        if (argv[i + 1] == 0)
        {
            print_redirection_syntax_error(argv[i]);
            return -1;
        }

        if (input && redirect_fd(STDIN_FD, argv[i + 1], O_RDONLY, 0) < 0)
            return -1;
        if (output && redirect_fd(STDOUT_FD, argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0) < 0)
            return -1;
        if (append && redirect_fd(STDOUT_FD, argv[i + 1], O_WRONLY | O_CREAT, 1) < 0)
            return -1;
        remove_arg_pair(argv, i);
    }
    return 0;
}

static int run_simple(char **argv)
{
    int status = -1;
    int pid = syscall(SYS_fork, 0, 0, 0);
    if (pid < 0)
    {
        write_str("Fork failed!\n");
        return -1;
    }
    if (pid == 0)
    {
        if (apply_redirections(argv) < 0)
            syscall(SYS_exit, -1, 0, 0);
        exec_command(argv);
    }
    wait_for_child(&status);
    return status;
}

static int split_pipeline(char **argv, char ***stages)
{
    int count = 1;

    stages[0] = argv;
    if (argv[0] == 0)
        return 0;

    for (int i = 0; argv[i]; i++)
    {
        if (str_cmp(argv[i], "|") != 0)
            continue;

        if (i == 0 || argv[i + 1] == 0 || count >= MAX_STAGES)
        {
            write_str("syntax error: invalid pipe\n");
            return -1;
        }
        argv[i] = 0;
        stages[count++] = &argv[i + 1];
    }
    return count;
}

static int run_pipeline(char ***stages, int stage_count)
{
    int prev_read = -1;
    int child_count = 0;
    int last_pid = -1;
    int last_pipe_status = -1;

    for (int i = 0; i < stage_count; i++)
    {
        int fds[2] = {-1, -1};
        int pid;

        if (i + 1 < stage_count && syscall(SYS_pipe, (uint64)fds, 0, 0) < 0)
        {
            write_str("Pipe creation failed!\n");
            return -1;
        }

        pid = syscall(SYS_fork, 0, 0, 0);
        if (pid < 0)
        {
            write_str("Fork failed!\n");
            if (fds[0] >= 0)
                syscall(SYS_close, fds[0], 0, 0);
            if (fds[1] >= 0)
                syscall(SYS_close, fds[1], 0, 0);
            return -1;
        }

        if (pid == 0)
        {
            if (prev_read >= 0)
            {
                syscall(SYS_close, STDIN_FD, 0, 0);
                syscall(SYS_dup, prev_read, 0, 0);
            }
            if (i + 1 < stage_count)
            {
                syscall(SYS_close, STDOUT_FD, 0, 0);
                syscall(SYS_dup, fds[1], 0, 0);
            }
            if (prev_read >= 0)
                syscall(SYS_close, prev_read, 0, 0);
            if (fds[0] >= 0)
                syscall(SYS_close, fds[0], 0, 0);
            if (fds[1] >= 0)
                syscall(SYS_close, fds[1], 0, 0);
            if (apply_redirections(stages[i]) < 0)
                syscall(SYS_exit, -1, 0, 0);
            exec_command(stages[i]);
        }

        child_count++;
        last_pid = pid;
        if (prev_read >= 0)
            syscall(SYS_close, prev_read, 0, 0);
        if (fds[1] >= 0)
            syscall(SYS_close, fds[1], 0, 0);
        prev_read = fds[0];
    }

    if (prev_read >= 0)
        syscall(SYS_close, prev_read, 0, 0);

    for (int i = 0; i < child_count; i++)
    {
        int status;
        int pid = wait_for_child(&status);
        if (pid == last_pid)
            last_pipe_status = status;
    }
    return last_pipe_status;
}

static void dispatch_command(int argc, char **argv)
{
    char **stages[MAX_STAGES];
    int stage_count;
    int builtin_status;

    if (argc == 0)
        return;

    if (find_token(argv, "|") < 0 && find_token(argv, "<") < 0 &&
        find_token(argv, ">") < 0 && find_token(argv, ">>") < 0 &&
        run_builtin(argc, argv, &builtin_status))
    {
        last_status = builtin_status;
        return;
    }

    stage_count = split_pipeline(argv, stages);
    if (stage_count < 0)
    {
        last_status = 1;
        return;
    }

    for (int i = 0; i < stage_count; i++)
    {
        if (validate_redirections(stages[i]) < 0)
        {
            last_status = 1;
            return;
        }
    }

    if (stage_count == 1)
        last_status = run_simple(stages[0]);
    else
        last_status = run_pipeline(stages, stage_count);
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
        expand_args(argc, argv);
        dispatch_command(argc, argv);
    }
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
