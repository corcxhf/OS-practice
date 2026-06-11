#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);
int str_len(const char *s);
const char msg[] = "hello\n";
void main(int argc, char *argv[])
{
    int fd[2];
    char buf[16];

    // 1. 发起 SYS_pipe 系统调用，传入数组地址
    int r = (int)syscall(SYS_pipe, (uint64)fd, 0, 0);
    if (r < 0)
    {
        char err[] = "pipe: failed to create pipe\n";
        syscall(SYS_write, 1, (uint64)err, (uint64)str_len(err));
        syscall(SYS_exit, (uint64)-1, 0, 0);
    }

    // 2. 孵化子进程
    int pid = (int)syscall(SYS_fork, 0, 0, 0);
    if (pid < 0)
    {
        char err[] = "pipe: fork failed\n";
        syscall(SYS_write, 1, (uint64)err, (uint64)str_len(err));
        syscall(SYS_exit, (uint64)-1, 0, 0);
    }

    if (pid == 0)
    {
        // ==========================================================
        // 👶 子进程阶段：负责写入
        // ==========================================================
        // 管道铁律：子进程只负责写，赶紧把不需要的读端关掉
        syscall(SYS_close, (uint64)fd[0], 0, 0);

        syscall(SYS_write, (uint64)fd[1], (uint64)msg, (uint64)str_len(msg));

        // 写完关闭写端，触发 EOF
        syscall(SYS_close, (uint64)fd[1], 0, 0);
        syscall(SYS_exit, 0, 0, 0);
    }
    else
    {
        // ==========================================================
        // 🧔 父进程阶段：负责读取
        // ==========================================================
        // 管道铁律：父进程只负责读，赶紧把不需要的写端关掉
        syscall(SYS_close, (uint64)fd[1], 0, 0);

        // 清空本地缓冲区
        for (int i = 0; i < 16; i++)
            buf[i] = 0;

        // 从管道读端拉取数据
        int nread = (int)syscall(SYS_read, (uint64)fd[0], (uint64)buf, 6);
        if (nread > 0)
        {
            // 成功拿到数据，顺手通过 1 号(控制台)输出到屏幕上！
            char prefix[] = "Pipe received: ";
            syscall(SYS_write, 1, (uint64)prefix, (uint64)str_len(prefix));
            syscall(SYS_write, 1, (uint64)buf, (uint64)nread);
        }

        // 读完关闭读端
        syscall(SYS_close, (uint64)fd[0], 0, 0);

        // 帮子进程收尸
        int status;
        syscall(SYS_wait, (uint64)&status, 0, 0);
        syscall(SYS_exit, 0, 0, 0);
    }
}

int str_len(const char *s)
{
    int len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

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