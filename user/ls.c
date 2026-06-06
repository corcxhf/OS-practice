#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"

struct dirent
{
    unsigned short inum; // inode 编号
    char name[14];       // 文件名
};
int str_len(const char *s);
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2);

void main(int argc, char *argv[])
{
    struct dirent de;
    struct stat st;

    char *path = ".";
    if (argc > 1)
    {
        path = argv[1];
    }

    // 1. SYS_open: 需要 3 个参数 (path, mode, flags)，刚刚好填满 a0, a1, a2
    // 注意把返回的 uint64 强转回 int，否则 fd < 0 永远不成立！
    int fd = (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);
    if (fd < 0)
    {
        char err[] = "ls: cannot open path\n";
        syscall(SYS_write, 1, (uint64)err, (uint64)str_len(err));
        // SYS_exit 只需要 1 个状态码参数，后面的 a1, a2 必须硬塞两个 0 补齐
        syscall(SYS_exit, (uint64)-1, 0, 0);
    }

    // ==========================================================
    // 🚨 工业级防线：调用你刚刚写好的 SYS_fstat
    // SYS_fstat 只需要 fd 和 &st 两个参数，a2 位置补 0
    // ==========================================================
    if ((int)syscall(SYS_fstat, (uint64)fd, (uint64)&st, 0) < 0)
    {
        syscall(SYS_close, (uint64)fd, 0, 0);
        syscall(SYS_exit, (uint64)-1, 0, 0);
    }

    // 看清体检报告，分类处理！
    if (st.type == T_FILE)
    {
        // 如果是普通文件，直接打印文件名
        syscall(SYS_write, 1, (uint64)path, (uint64)str_len(path));
        char space = ' ';
        syscall(SYS_write, 1, (uint64)&space, 1);
    }
    else if (st.type == T_DIR)
    {
        // 如果是目录，进入读目录项循环 (SYS_read 需要 3 个参数，填满)
        while ((int)syscall(SYS_read, (uint64)fd, (uint64)&de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            syscall(SYS_write, 1, (uint64)de.name, (uint64)str_len(de.name));
            char space = ' ';
            syscall(SYS_write, 1, (uint64)&space, 1);
        }
    }

    char newline = '\n';
    syscall(SYS_write, 1, (uint64)&newline, 1);

    // 优雅退出 (SYS_close 和 SYS_exit 均要补 0)
    syscall(SYS_close, (uint64)fd, 0, 0);
    syscall(SYS_exit, 0, 0, 0);
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
