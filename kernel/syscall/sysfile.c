#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "fs.h"
#include "types.h"

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
extern int piperead(struct pipe *pi, uint64 addr, int n);
struct inode;
extern void iunlockput(struct inode *ip);

extern struct file *filealloc();
extern void fileclose(struct file *f);
extern int fdalloc(struct file *f);
extern void wakeup(void *chan);
extern void sleep(void *chan, struct spinlock *lk);

extern void acquire(struct spinlock *);
extern void release(struct spinlock *lk);
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
    // printf("[DEBUG] sys_open called! path: %s, omode: %d\n", path, flags);
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

pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);

uint64 sys_read(void)
{
    struct file *f;
    int n, fd;
    uint64 addr;
    struct proc *p = myproc();

    if (argint(0, &fd) < 0 || argint(2, &n) < 0 || argaddr(1, &addr) < 0)
        return -1;
    if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;

    if (f->readable == 0)
        return -1;

    if (f->type == FD_CONSOLE)
    {
        uint64 user_addr = addr;
        int i;

        for (i = 0; i < n; i++)
        {
            extern int consgetc(void);
            int c = consgetc();
            if (c < 0)
                break;
            uint64 va_page = PGROUNDDOWN(user_addr);
            uint64 offset = user_addr - va_page;

            pte_t *pte = walk(p->pagetable, va_page, 0);
            if (pte == 0 || (*pte & PTE_V) == 0)
                return -1;

            uint64 pa = PTE2PA(*pte);
            char *kernel_ptr = (char *)(pa + offset);

            *kernel_ptr = (char)c;
            user_addr++;
            if (c == '\n')
            {
                i++; // 把当前这个 '\n' 算进成功读取的字节数里
                break;
            }
        }
        return i;
    }
    else if (f->type == FD_INODE)
    {
        return fileread(f, n, (char *)addr);
    }
    else if (f->type == FD_PIPE)
    {
        return piperead(f->pipe, addr, n);
    }

    return -1;
}
uint64 sys_close(void)
{
    int fd;
    struct file *f;
    struct proc *p = myproc();

    if (argint(0, &fd) < 0 || fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;
    p->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

int copyinstr(char *dst, uint64 src, int max)
{
    char *s = (char *)src;
    int i;

    for (i = 0; i < max; i++)
    {
        dst[i] = s[i];
        if (s[i] == '\0')
            return 0; // 成功
    }
    return -1;
}

int pipewrite(struct pipe *pi, uint64 addr, int n)
{
    int i = 0;
    // 🚨 这里的 addr 其实是 sys_write 传进来的内核 buf 地址！
    // 我们直接把它强转成普通的内核字符指针，不需要查表！
    char *kernel_buf = (char *)addr;

    acquire(&pi->lock);

    while (i < n)
    {
        // 嫌疑点 A：如果读端全关了，写数据就没有任何意义了
        if (pi->readopen == 0)
        {
            release(&pi->lock);
            return -1;
        }

        // 嫌疑点 B：管道满了
        if (pi->nwrite - pi->nread == PIPESIZE)
        {
            wakeup(&pi->nread);
            sleep(&pi->nwrite, &pi->lock);
        }
        else
        {
            // 🚨 终极修复：直接从 kernel_buf 读取数据，写入管道环形缓冲区！
            pi->data[pi->nwrite % PIPESIZE] = kernel_buf[i];
            pi->nwrite++;
            i++;
        }
    }

    wakeup(&pi->nread);
    release(&pi->lock);

    return i;
}

int piperead(struct pipe *pi, uint64 addr, int n)
{
    int i = 0;
    struct proc *p = myproc();

    acquire(&pi->lock);

    // 嫌疑点 A：管道是空的 (nwrite == nread)
    while (pi->nwrite == pi->nread)
    {
        // 如果管道空了，且写端已经被全部 close 掉了
        if (pi->writeopen == 0)
        {
            // 这就是真正的文件末尾 (EOF)，cat 读到 0 就会优雅退出
            release(&pi->lock);
            return 0;
        }
        // 如果写端没关，纯粹是还没来得及写数据，读端就睡在 pi->nread 上
        wakeup(&pi->nwrite); // 顺手唤醒可能因为管满而卡住的写端
        sleep(&pi->nread, &pi->lock);
    }

    // 2. 管道里有数据，开始读
    while (i < n && pi->nwrite != pi->nread)
    {
        // 逐字节利用 walk 找到用户态传进来的接收缓冲区虚拟地址 addr + i
        uint64 va_page = PGROUNDDOWN(addr + i);
        uint64 offset = (addr + i) - va_page;

        pte_t *pte = walk(p->pagetable, va_page, 0);
        if (pte == 0 || (*pte & PTE_V) == 0)
        {
            release(&pi->lock);
            return -1;
        }
        uint64 pa = PTE2PA(*pte);
        char *user_ptr = (char *)(pa + offset);

        // 从环形缓冲区中取出数据，写回用户态宇宙
        *user_ptr = pi->data[pi->nread % PIPESIZE];
        pi->nread++;
        i++;
    }

    // 读完了，唤醒可能因为管满而卡住的写端
    wakeup(&pi->nwrite);
    release(&pi->lock);

    return i; // 返回真正读到的字节数
}

void pipeclose(struct pipe *pi, int writable)
{
    acquire(&pi->lock);

    if (writable)
    {
        pi->writeopen = 0;  // 🚨 宣告写端彻底死亡！
        wakeup(&pi->nread); // 🚨 极其关键：必须叫醒正在死等的读端（比如 cat），让它起来看到 writeopen==0 并返回 0！
    }
    else
    {
        pi->readopen = 0;    // 宣告读端死亡
        wakeup(&pi->nwrite); // 叫醒可能因为管子满了卡住的写端
    }

    // 如果读写两端都死透了，彻底释放管道这页物理内存
    if (pi->readopen == 0 && pi->writeopen == 0)
    {
        release(&pi->lock);
        kfree((void *)pi); // 回收内存
    }
    else
    {
        release(&pi->lock);
    }
}