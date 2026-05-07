#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "fs.h"
#include "types.h"

#ifndef O_CREAT
#define O_CREAT 0x200
#endif
#ifndef O_WRONLY
#define O_WRONLY 0x001
#endif
#ifndef O_RDONLY
#define O_RDONLY 0x000
#endif
#ifndef O_RDWR
#define O_RDWR 0x002
#endif

// /* 文件系统布局参数 */
// #define BSIZE 1024                       /* 磁盘块大小（字节）*/
// #define NDIRECT 12                       /* 直接块指针数量 */
// #define NINDIRECT (BSIZE / sizeof(uint)) /* 一级间接块中的指针数量 */
// #define DIRSIZ 14                        /* 目录项中文件名的最大长度 */
// #define IPB (BSIZE / sizeof(struct dinode))

extern int argint(int n, int *ip);
extern int argstr(int n, char *buf, int max);

int argaddr(int n, uint64 *ap);
extern int fileread(struct file *f, int n, char *buf);
struct inode;
extern void iunlockput(struct inode *ip);

extern struct file *filealloc();
extern void fileclose(struct file *f);
extern int fdalloc(struct file *f);

uint64 sys_open(void)
{
    char path[MAXPATH];
    int fd, flags;
    struct file *f;
    struct inode *ip;

    /* 1. 从 trapframe 读取参数 (path 虚拟地址和 flags) */
    /* 2. 调用 copyinstr 将用户态 path 复制到内核缓冲区 */
    // 注意：在 xv6 框架中，argstr 内部通常已经处理了 copyinstr
    if (argstr(0, path, MAXPATH) < 0 || argint(1, &flags) < 0)
        return -1;

    if (flags & O_CREAT)
    {
        /* 3. 若 O_CREAT 标志设置：创建文件逻辑 */
        char name[DIRSIZ];
        struct inode *dp;

        // 调用 nameiparent() 分离父目录路径和文件名
        if ((dp = nameiparent(path, name)) == 0)
            return -1;

        ilock(dp);

        // 在父目录中查找是否已存在
        if ((ip = dirlookup(dp, name, 0)) == 0)
        {
            /* 若不存在：分配新 inode 并注册 */
            if ((ip = ialloc(dp->dev, T_FILE)) == 0)
            {
                iunlockput(dp);
                return -1;
            }
            ilock(ip);
            ip->nlink = 1;
            iupdate(ip); // 将新 inode 信息同步到磁盘

            if (dirlink(dp, name, ip->inum) < 0)
            {
                iunlockput(ip);
                iunlockput(dp);
                return -1;
            }
            iunlockput(dp); // 创建完成，释放父目录
        }
        else
        {
            /* 若存在：直接使用 */
            iunlockput(dp);
            ilock(ip);
            // 如果已存在的是目录且要求写入，通常是不允许的
            if (ip->type == T_DIR && (flags & (O_WRONLY | O_RDWR)))
            {
                iunlockput(ip);
                return -1;
            }
        }
    }
    else
    {
        /* 4. 若无 O_CREAT：直接查找文件 */
        if ((ip = namei(path)) == 0)
            return -1;

        ilock(ip);
        // 权限检查：目录不能以写方式打开
        if (ip->type == T_DIR && (flags & (O_WRONLY | O_RDWR)))
        {
            iunlockput(ip);
            return -1;
        }
    }

    /* 到此为止，ip 已锁定且是目标文件的有效引用 */

    /* 5. 分配 file 对象 */
    if ((f = filealloc()) == 0)
    {
        iunlockput(ip);
        return -1;
    }

    /* 6. 在 proc.ofile[] 中找空闲槽位，记录 fd */
    if ((fd = fdalloc(f)) < 0)
    {
        fileclose(f); // 会内部调用 iput
        iunlock(ip);
        return -1;
    }

    /* 7. 初始化 file 结构 */
    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(flags & O_WRONLY); // 默认读，除非只写
    f->writable = ((flags & O_WRONLY) || (flags & O_RDWR));

    /* 8. 返回 fd，保持 inode 的引用计数但解锁 */
    iunlock(ip);

    return fd;
}
uint64
sys_read(void)
{
    struct file *f;
    int n, fd;
    uint64 addr;

    /* 1. 从 trapframe 读取参数 */
    /* argfd 是 xv6 的常用辅助函数：它同时完成了获取 fd 和检查合法性（第 2 步） */
    /* 参数 0 是 fd，参数 1 是 addr，参数 2 是 n */
    argint(0, &fd), argint(2, &n), argaddr(1, &addr);
    f = myproc()->ofile[fd];

    /* 3. 调用 fileread() 并返回实际读取字节数 */
    return fileread(f, n, (char *)addr);
}

uint64 sys_close(void)
{
    int fd;
    struct file *f;
    struct proc *p = myproc(); // 获取当前进程结构体

    /* 1. 获取用户传入的第一个参数 fd */
    /* 同时检查：fd 是否在合法范围 (0 ~ NOFILE) 且该槽位不为空 */
    if (argint(0, &fd) < 0 || fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;

    /* 2. 在进程的文件表中将该槽位清空 */
    /* 这样该 fd 就可以在下次 open 时被重新分配 */
    p->ofile[fd] = 0;

    /* 3. 调用文件层的通用关闭函数 */
    /* fileclose 会负责处理 f->ref--，并在引用归零时释放 inode 或 pipe */
    fileclose(f);

    return 0;
}

int copyinstr(char *dst, uint64 src, int max)
{
    char *s = (char *)src; // 因为共用页表，直接强制转换指针
    int i;

    for (i = 0; i < max; i++)
    {
        dst[i] = s[i];

        // 如果遇到了字符串结束符，说明复制完成
        if (s[i] == '\0')
            return 0; // 成功
    }

    // 如果遍历了 max 个字符还没遇到 \0，说明路径太长
    return -1;
}