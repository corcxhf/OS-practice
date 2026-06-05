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
        while (1)
        {
            int n = syscall(SYS_read, 0, (uint64)(buf + buf_idx), 1);
            if (n <= 0)
                continue;

            char current_char = buf[buf_idx];
            if (current_char == '\n')
            {
                buf[buf_idx] = '\0';
                break;
            }
            else if (current_char == 127 || current_char == '\b')
            {
                if (buf_idx > 0)
                    buf_idx--;
            }
            else
            {
                if (buf_idx < 127)
                    buf_idx++;
            }
        }

        if (buf_idx == 0)
            continue;

        argc = parse_args(buf, argv);
        if (argc == 0)
            continue;

        if (str_cmp(buf, "help") == 0)
        {
            int len = str_len((const char *)p_help);
            syscall(SYS_write, 1, p_help, len);
        }
        else if (str_cmp(buf, "clear") == 0)
        {
            syscall(SYS_write, 1, p_clear, 7);
        }
        else if (str_cmp(argv[0], "echo") == 0)
        {
            if (argc == 1)
            {
                char echo_usage[] = "Echo what?\n";
                syscall(SYS_write, 1, (uint64)echo_usage, str_len(echo_usage));
            }
            else
            {
                for (int i = 1; i < argc; i++)
                {
                    syscall(SYS_write, 1, (uint64)argv[i], str_len(argv[i]));
                    if (i < argc - 1)
                    {
                        char space[] = " ";
                        syscall(SYS_write, 1, (uint64)space, 1);
                    }
                }
                syscall(SYS_write, 1, p_newline, 1);
            }
        }
        else
            syscall(SYS_write, 1, p_unknown, 30);
        
    }
}

void __attribute__((naked, section(".text.entry"))) _start()
{
    __asm__ volatile("call main");
}