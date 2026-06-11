// user/initcode.c
typedef unsigned long uint64;

#include "proc.h"
#define BUF_SIZE 128

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

int str_len(const char *s)
{
    int len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

int str_cmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int parse_args(char *buf, char **argv)
{
    int argc = 0;
    int in_token = 0;

    for (int i = 0; buf[i] != '\0'; i++)
    {
        if (buf[i] == ' ' || buf[i] == '\t')
        {
            buf[i] = '\0';
            in_token = 0;
        }
        else if (!in_token)
        {
            if (argc < 16)
                argv[argc++] = &buf[i];
            in_token = 1;
        }
    }
    return argc;
}

void update_path(char *cwd, const char *target)
{
    if (target[0] == '/')
    {
        int i = 0;
        while (target[i] != '\0' && i < 127)
        {
            cwd[i] = target[i];
            i++;
        }
        cwd[i] = '\0';
    }
    else if (str_cmp(target, "..") == 0)
    {
        int len = str_len(cwd);
        if (len > 1)
        {
            int i = len - 1;
            while (i > 0 && cwd[i] != '/')
                i--;
            if (i == 0)
                cwd[1] = '\0';
            else
                cwd[i] = '\0';
        }
    }
    else if (str_cmp(target, ".") == 0)
    {
    }
    else
    {
        int len = str_len(cwd);
        if (cwd[len - 1] != '/')
            cwd[len++] = '/';
        int i = 0;
        while (target[i] != '\0' && len < 127)
        {
            cwd[len++] = target[i];
            i++;
        }
        cwd[len] = '\0';
    }
}

// 🚨 瘦身核心：抽离执行逻辑，大幅减少生成的汇编代码体积！
void execute_cmd(char **argv)
{
    syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);
    if (argv[0][0] != '/')
    {
        char abs_path[64];
        abs_path[0] = '/';
        int j = 0;
        while (argv[0][j] != '\0' && j < 60)
        {
            abs_path[j + 1] = argv[0][j];
            j++;
        }
        abs_path[j + 1] = '\0';
        syscall(SYS_exec, (uint64)abs_path, (uint64)argv, 0);
    }
    char exec_err[] = "Command not found!\n";
    syscall(SYS_write, 1, (uint64)exec_err, str_len(exec_err));
    syscall(SYS_exit, -1, 0, 0);
}

const char prompt[] = "MyOS:/> ";
const char help_msg[] = "Available commands:\n  help - Show this message\n  echo - Repeat input\n  clear- Clear screen\n";
const char newline[] = "\n";
const char unknown[] = "Unknown command. Type 'help'.\n";
const char back_seq[] = {'\b', ' ', '\b'};
const char clear_seq[] = {27, '[', '2', 'J', 27, '[', 'H'};
const char echo_stub[] = "Echo what?\n";

char input_buf[128];

void main()
{
    uint64 p_prompt, p_help, p_newline, p_unknown, p_buf, p_back, p_clear, p_echo;
    __asm__ volatile("lla %0, prompt" : "=r"(p_prompt));
    __asm__ volatile("lla %0, help_msg" : "=r"(p_help));
    __asm__ volatile("lla %0, newline" : "=r"(p_newline));
    __asm__ volatile("lla %0, unknown" : "=r"(p_unknown));
    __asm__ volatile("lla %0, input_buf" : "=r"(p_buf));
    __asm__ volatile("lla %0, back_seq" : "=r"(p_back));
    __asm__ volatile("lla %0, clear_seq" : "=r"(p_clear));
    __asm__ volatile("lla %0, echo_stub" : "=r"(p_echo));

    char *buf = (char *)p_buf;
    int buf_idx = 0;
    char *argv[16];
    int argc = 0;
    char cwd_path[128] = "/";
    while (1)
    {
        char prompt_head[] = "MyOS:";
        char prompt_tail[] = "> ";
        syscall(SYS_write, 1, (uint64)prompt_head, str_len(prompt_head));
        syscall(SYS_write, 1, (uint64)cwd_path, str_len(cwd_path));
        syscall(SYS_write, 1, (uint64)prompt_tail, str_len(prompt_tail));

        buf_idx = 0;
        int cursor = 0;
        int shell_esc_state = 0;
        const char c_left[] = {27, '[', 'D'};
        const char c_right[] = {27, '[', 'C'};
        for (int i = 0; i < 16; i++)
        {
            argv[i] = 0;
        }
        while (1)
        {
            char c;
            int n = syscall(SYS_read, 0, (uint64)&c, 1);
            if (n <= 0)
                continue;

            if (c == 27)
            {
                shell_esc_state = 1;
                continue;
            }
            if (shell_esc_state == 1 && c == '[')
            {
                shell_esc_state = 2;
                continue;
            }
            if (shell_esc_state == 2)
            {
                shell_esc_state = 0;
                if (c == 'D')
                {
                    if (cursor > 0)
                    {
                        cursor--;
                        syscall(SYS_write, 1, (uint64)c_left, 3);
                    }
                }
                else if (c == 'C')
                {
                    if (cursor < buf_idx)
                    {
                        cursor++;
                        syscall(SYS_write, 1, (uint64)c_right, 3);
                    }
                }
                continue;
            }
            if (shell_esc_state != 0 && c != 27 && c != '[')
                shell_esc_state = 0;

            if (c == '\n')
            {
                buf[buf_idx] = '\0';
                syscall(SYS_write, 1, p_newline, 1);
                break;
            }
            else if (c == 127 || c == '\b')
            {
                if (cursor > 0)
                {
                    int move_len = buf_idx - cursor;
                    for (int i = 0; i < move_len; i++)
                        buf[cursor - 1 + i] = buf[cursor + i];
                    cursor--;
                    buf_idx--;
                    syscall(SYS_write, 1, (uint64)c_left, 3);
                    if (move_len > 0)
                        syscall(SYS_write, 1, (uint64)(buf + cursor), move_len);
                    char space = ' ';
                    syscall(SYS_write, 1, (uint64)&space, 1);
                    for (int i = 0; i < move_len + 1; i++)
                        syscall(SYS_write, 1, (uint64)c_left, 3);
                }
            }
            else
            {
                if (buf_idx < 127)
                {
                    int move_len = buf_idx - cursor;
                    for (int i = move_len - 1; i >= 0; i--)
                        buf[cursor + 1 + i] = buf[cursor + i];
                    buf[cursor] = c;
                    buf_idx++;
                    syscall(SYS_write, 1, (uint64)(buf + cursor), move_len + 1);
                    cursor++;
                    for (int i = 0; i < move_len; i++)
                        syscall(SYS_write, 1, (uint64)c_left, 3);
                }
            }
        }

        if (buf_idx == 0)
            continue;
        argc = parse_args(buf, argv);
        if (argc == 0)
            continue;
        if (str_cmp(argv[0], "cd") == 0)
        {
            if (argc < 2)
                continue;
            if ((int)syscall(SYS_chdir, (uint64)argv[1], 0, 0) == 0)
                update_path(cwd_path, argv[1]);
            else
            {
                char cd_err[] = "cd: cannot cd\n";
                syscall(SYS_write, 1, (uint64)cd_err, str_len(cd_err));
            }
            continue;
        }
        if (str_cmp(argv[0], "pwd") == 0)
        {
            syscall(SYS_write, 1, (uint64)cwd_path, str_len(cwd_path));
            syscall(SYS_write, 1, p_newline, 1);
            continue;
        }

        // ==========================================================
        // 🌟 1. 扫描管道
        // ==========================================================
        int pipe_idx = -1;
        for (int i = 0; argv[i] != 0; i++)
        {
            if (str_cmp(argv[i], "|") == 0)
            {
                pipe_idx = i;
                break;
            }
        }

        if (pipe_idx != -1)
        {
            argv[pipe_idx] = 0;
            char **right_argv = &argv[pipe_idx + 1];
            int p[2];

            if (syscall(SYS_pipe, (uint64)p, 0, 0) < 0)
            {
                char err[] = "Pipe creation failed!\n";
                syscall(SYS_write, 1, (uint64)err, str_len(err));
                continue;
            }

            int pid_left = syscall(SYS_fork, 0, 0, 0);
            if (pid_left == 0)
            {
                syscall(SYS_close, 1, 0, 0);
                syscall(SYS_dup, p[1], 0, 0);
                syscall(SYS_close, p[0], 0, 0);
                syscall(SYS_close, p[1], 0, 0);
                execute_cmd(argv); // 🚨 调用瘦身函数！
            }

            int pid_right = syscall(SYS_fork, 0, 0, 0);
            if (pid_right == 0)
            {
                syscall(SYS_close, 0, 0, 0);
                syscall(SYS_dup, p[0], 0, 0);
                syscall(SYS_close, p[1], 0, 0);
                syscall(SYS_close, p[0], 0, 0);
                execute_cmd(right_argv); // 🚨 调用瘦身函数！
            }

            syscall(SYS_close, p[0], 0, 0);
            syscall(SYS_close, p[1], 0, 0);

            int status;
            syscall(SYS_wait, (uint64)&status, 0, 0);
            syscall(SYS_wait, (uint64)&status, 0, 0);
            continue;
        }

        // ==========================================================
        // 🌟 2. 单一命令及重定向
        // ==========================================================
        int pid = syscall(SYS_fork, 0, 0, 0);
        if (pid < 0)
        {
            char fork_err[] = "Fork failed!\n";
            syscall(SYS_write, 1, (uint64)fork_err, str_len(fork_err));
        }
        else if (pid == 0)
        {
            int i = 0;
            while (argv[i] != 0)
            {
                if (str_cmp(argv[i], ">") == 0)
                {
                    if (argv[i + 1] == 0)
                    {
                        char err[] = "syntax error: expected file after >\n";
                        syscall(SYS_write, 1, (uint64)err, str_len(err));
                        syscall(SYS_exit, -1, 0, 0);
                    }
                    char *filename = argv[i + 1];
                    syscall(SYS_close, 1, 0, 0);
                    int fd = syscall(SYS_open, (uint64)filename, 0x201, 0);
                    if (fd < 0)
                        syscall(SYS_exit, -1, 0, 0);
                    argv[i] = 0;
                    break;
                }
                i++;
            }
            execute_cmd(argv); // 🚨 调用瘦身函数！
        }
        else
        {
            int status;
            syscall(SYS_wait, (uint64)&status, 0, 0);
        }
    }
}

void __attribute__((naked, section(".text.entry"))) _start()
{
    __asm__ volatile("call main");
}