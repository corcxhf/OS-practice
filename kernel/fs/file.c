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

    for (fd = 0; fd < NOFILE; fd++)
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

    /* 1. 权限检查（你已经写了） */
    if (f->writable == 0)
        return -1;

    /* 3. 加锁：写入前必须锁定 inode */
    ilock(f->ip);

    /* 4. 调用底层 writei 进行真正的磁盘/缓冲区写入 */
    /* 注意：第二个参数为 1，表示数据源来自用户空间 */

    if ((r = writei(f->ip, 1, addr, f->off, n)) > 0)
    {
        /* 5. 关键：更新该文件描述符的偏移量 */
        /* 这样下次 write 就会从当前结束的位置继续写 */
        f->off += r;
        ret = r;
    }
    else
    {
        ret = -1;
    }
    /* 6. 解锁：写完了，把 inode 释放给别人用 */
    iunlock(f->ip);

    return ret;
}