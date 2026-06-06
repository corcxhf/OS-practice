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

    while (1)
    {
        syscall(SYS_write, 1, p_prompt, 8);
        buf_idx = 0;
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
                // 'A' 和 'B' (上下键) 依然直接吞掉
                continue;
            }
            if (shell_esc_state != 0 && c != 27 && c != '[')
            {
                shell_esc_state = 0;
            }
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
                    {
                        buf[cursor - 1 + i] = buf[cursor + i];
                    }
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
        if (str_cmp(argv[0], "clear") == 0)
        {
            syscall(SYS_write, 1, p_clear, 7);
            continue;
        }
        int pid = syscall(SYS_fork, 0, 0, 0);

        if (pid < 0)
        {
            char fork_err[] = "Fork failed!\n";
            syscall(SYS_write, 1, (uint64)fork_err, str_len(fork_err));
        }
        else if (pid == 0)
        {
            syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);
            char exec_err[] = "exec failed!\n";
            syscall(SYS_write, 1, (uint64)exec_err, str_len(exec_err));
            syscall(SYS_exit, -1, 0, 0);
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