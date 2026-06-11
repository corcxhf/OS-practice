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

// 新增：在 shell 内部追踪和更新当前路径字符串
void update_path(char *cwd, const char *target)
{
    if (target[0] == '/')
    {
        // 1. 如果是绝对路径，直接覆盖
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
        // 2. 如果是退回上级，砍掉最后一截
        int len = str_len(cwd);
        if (len > 1)
        { // 只要不是在根目录 "/"
            int i = len - 1;
            // 往回找上一个 '/'
            while (i > 0 && cwd[i] != '/')
            {
                i--;
            }
            if (i == 0)
            {
                cwd[1] = '\0'; // 退到底了，变成 "/"
            }
            else
            {
                cwd[i] = '\0'; // 砍掉后面的
            }
        }
    }
    else if (str_cmp(target, ".") == 0)
    {
        // 3. 原地踏步，啥也不做
    }
    else
    {
        // 4. 进入子目录，拼接字符串
        int len = str_len(cwd);
        // 如果当前不是根目录，加个 '/' 作为分隔符
        if (cwd[len - 1] != '/')
        {
            cwd[len++] = '/';
        }
        int i = 0;
        while (target[i] != '\0' && len < 127)
        {
            cwd[len++] = target[i];
            i++;
        }
        cwd[len] = '\0';
    }
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
        if (str_cmp(argv[0], "cd") == 0)
        {
            // 如果用户只敲了 "cd" 没有带路径参数，可以选择忽略
            if (argc < 2)
            {
                continue;
            }

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

        int pid = syscall(SYS_fork, 0, 0, 0);

        if (pid < 0)
        {
            char fork_err[] = "Fork failed!\n";
            syscall(SYS_write, 1, (uint64)fork_err, str_len(fork_err));
        }
        else if (pid == 0)
        {
            // ==========================================================
            // 🚨 新增：解析并执行输出重定向 (>)
            // ==========================================================
            int i = 0;
            while (argv[i] != 0)
            {
                if (str_cmp(argv[i], ">") == 0)
                {
                    // 1. 语法校验：> 后面必须跟着文件名
                    if (argv[i + 1] == 0)
                    {
                        char err[] = "syntax error: expected file after >\n";
                        syscall(SYS_write, 1, (uint64)err, str_len(err));
                        syscall(SYS_exit, -1, 0, 0);
                    }

                    char *filename = argv[i + 1];

                    // 2. 狸猫换太子第一步：无情关闭标准输出 (fd = 1)
                    syscall(SYS_close, 1, 0, 0);

                    // 3. 狸猫换太子第二步：打开/创建目标文件
                    // 参数：文件名, O_CREAT(0x200) | O_WRONLY(0x001) = 0x201
                    // 内核会自动分配刚刚空出来的 1 号槽位给它！
                    int fd = syscall(SYS_open, (uint64)filename, 0x201, 0);
                    if (fd < 0)
                    {
                        // 此时 1 号槽位已关，屏幕打不出字了，只能自杀
                        syscall(SYS_exit, -1, 0, 0);
                    }

                    // 4. 毁尸灭迹：截断参数表！
                    // 把 ">" 变成 0 (NULL)，目标程序就永远看不见重定向符号了
                    argv[i] = 0;
                    break;
                }
                i++;
            }

            // ==========================================================
            // 下面是你原有的 exec 逻辑（完全保持不变！）
            // ==========================================================
            // 第 1 次尝试：原汁原味地执行
            syscall(SYS_exec, (uint64)argv[0], (uint64)argv, 0);

            // 如果走到这里，说明第 1 次尝试失败了！
            // 第 2 次尝试：如果它不是以 '/' 开头，我们强行给它加上 '/' 去根目录找！
            if (argv[0][0] != '/')
            {
                char abs_path[64];
                abs_path[0] = '/';
                int j = 0; // 注意：把这里的 i 改成了 j，避免和上面的循环变量冲突
                // 把 argv[0] 拼接到 '/' 后面
                while (argv[0][j] != '\0' && j < 60)
                {
                    abs_path[j + 1] = argv[0][j];
                    j++;
                }
                abs_path[j + 1] = '\0';

                // 拿着拼好的绝对路径 (比如 "/ls") 再次尝试执行！
                syscall(SYS_exec, (uint64)abs_path, (uint64)argv, 0);
            }

            // 如果两次都失败了，那才是真正的找不到命令
            // 注意：如果发生了重定向，这行报错会被写进目标文件里，这是正常的 Unix 行为
            char exec_err[] = "Command not found!\n";
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