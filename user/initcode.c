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
                // 只有当光标不在行首（左侧有字可删）时才处理
                if (cursor > 0)
                {
                    int move_len = buf_idx - cursor; // 需要向前平移的字符数

                    // A. 账本同步：数据结构向前挤压
                    for (int i = 0; i < move_len; i++)
                    {
                        buf[cursor - 1 + i] = buf[cursor + i];
                    }
                    cursor--;
                    buf_idx--;

                    // B. 屏幕动态渲染魔法：
                    // 1. 先把光标往左退一格
                    syscall(SYS_write, 1, (uint64)c_left, 3);
                    // 2. 刷新打印光标后面所有挪过来的字符
                    if (move_len > 0)
                    {
                        syscall(SYS_write, 1, (uint64)(buf + cursor), move_len);
                    }
                    // 3. 打印一个空格，把屁股后面多出来的那个历史残留字符擦掉
                    char space = ' ';
                    syscall(SYS_write, 1, (uint64)&space, 1);

                    for (int i = 0; i < move_len + 1; i++)
                    {
                        syscall(SYS_write, 1, (uint64)c_left, 3);
                    }
                }
            }

            else
            {
                if (buf_idx < 127)
                {
                    int move_len = buf_idx - cursor; // 需要向后腾出位置的字符数

                    // A. 账本同步：从后往前倒车，空出 cursor 这个座位
                    for (int i = move_len - 1; i >= 0; i--)
                    {
                        buf[cursor + 1 + i] = buf[cursor + i];
                    }

                    // 塞入新键盘字符
                    buf[cursor] = c;
                    buf_idx++;

                    // B. 屏幕动态渲染魔法：
                    // 1. 打印当前插入的字符，以及后面所有被挤向后方的字符
                    syscall(SYS_write, 1, (uint64)(buf + cursor), move_len + 1);

                    cursor++; // 游标前进

                    for (int i = 0; i < move_len; i++)
                    {
                        syscall(SYS_write, 1, (uint64)c_left, 3);
                    }
                }
            }
        }
        if (buf_idx == 0)
            continue;

        argc = parse_args(buf, argv);
        if (argc == 0)
            continue;

        // 2. 依然保留一些不需要 fork 的内建命令（比如 clear）
        if (str_cmp(argv[0], "clear") == 0)
        {
            syscall(SYS_write, 1, p_clear, 7);
            continue;
        }

        // 3. 凡是外部磁盘命令（如 echo），果断祭出 Unix 三连击！
        int pid = syscall(SYS_fork, 0, 0, 0);

        if (pid < 0)
        {
            char fork_err[] = "Fork failed!\n";
            syscall(SYS_write, 1, (uint64)fork_err, str_len(fork_err));
        }
        else if (pid == 0)
        {
            // =====================================================
            // 【子进程分支 (Child)】
            // =====================================================
            // SYS_exec 需要 2 个参数：路径和 argv 数组。最后一个 a2 补 0
            syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);

            // 如果执行到这里，说明 exec 依然失败了
            char exec_err[] = "exec failed!\n";
            syscall(SYS_write, 1, (uint64)exec_err, str_len(exec_err));
            syscall(SYS_exit, -1, 0, 0);
        }
        else
        {
            // =====================================================
            // 【父进程分支 (Shell 本尊)】
            // =====================================================
            int status;
            // SYS_wait 需要 1 个参数：接收状态的内核/用户指针。后面两个 a1, a2 补 0
            syscall(SYS_wait, (uint64)&status, 0, 0);
        }
    }
}

void __attribute__((naked, section(".text.entry"))) _start()
{
    __asm__ volatile("call main");
}