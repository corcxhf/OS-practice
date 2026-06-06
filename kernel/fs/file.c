#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include "fs.h"
#include "proc.h"
struct
{
    struct file file[NFILE]; // NFILE 在 param.h 中定义（如64）

} ftable;

extern void iput(struct inode *ip);
extern void ilock(struct inode *ip);
extern int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
extern void iunlock(struct inode *ip);
extern void stati(struct inode *ip, struct stat *st);
struct file *filealloc()
{
    struct file *f;
    for (int i = 0; i < NFILE; i++)
    {
        f = &ftable.file[i];
        if (f->ref == 0)
        {
            f->ref = 1;
            return f;
        }
    }
    return 0;
}

int fdalloc(struct file *f)
{
    int fd;
    struct proc *p = myproc();

    for (fd = 3; fd < NOFILE; fd++)
    {
        /* 如果找到一个空槽位 */
        if (p->ofile[fd] == 0)
        {
            p->ofile[fd] = f;
            return fd;
        }
    }

    return -1; // 进程打开的文件数超过了 NOFILE 上限
}

struct file *filedup(struct file *f)
{
    f->ref++;
    return f;
}

void fileclose(struct file *f)
{
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0)
        return; // 如果还有人在用，直接返回

    // 只有 ref == 0 了才真正释放
    iput(f->ip);
    f->type = 0;
    f->ip = 0;
    f->off = 0;
}

int fileread(struct file *f, int n, char *buf)
{
    if (!f->readable)
        return -1;
    ilock(f->ip);

    int read_byte = readi(f->ip, 1, (uint64)buf, f->off, n);
    f->off += read_byte;
    iunlock(f->ip);
    return read_byte;
}

int filewrite(struct file *f, uint64 addr, int n)
{
    int r, ret = 0;
    if (f->writable == 0)
        return -1;
    ilock(f->ip);
    if ((r = writei(f->ip, 1, addr, f->off, n)) > 0)
    {
        f->off += r;
        ret = r;
    }
    else
        ret = -1;

    iunlock(f->ip);

    return ret;
}

int filestat(struct file *f, uint64 addr)
{
    struct proc *p = myproc();
    struct stat st;

    if (f->type == FD_INODE || f->type == FD_DEVICE)
    {
        ilock(f->ip);      // 锁住 Inode 防止并发修改
        stati(f->ip, &st); // 这个是底层函数，负责把 Inode 里的 size/type 抄到 st 里
        iunlock(f->ip);

        // ==========================================================
        // 🚨 核心防线：用 copyout 安全越过页表，把体检报告交到用户手上！
        // ==========================================================
        if (copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
            return -1;
        return 0;
    }
    return -1;
}