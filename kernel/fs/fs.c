/* fs.c — 文件系统核心（Lab7 任务2-3）
 *
 * 实现"文件系统层"，在块设备缓冲层之上提供：
 *   - bmap()        : 文件逻辑块号 → 磁盘物理块号（处理直接/间接索引）
 *   - readi()       : 从 inode 文件读取数据
 *   - writei()      : 向 inode 文件写入数据
 *   - dirlookup()   : 在目录中按文件名查找 inode
 *
 * 重要概念：
 *   Inode（索引节点）= 文件的"灵魂"，存储文件大小、数据块位置等元信息
 *   Dirent（目录项）= 目录文件的内容，格式：{inum(2字节), name(14字节)}
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include "fs.h"
#include "proc.h"
extern void virtio_disk_init(void);
extern int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
extern void acquire(struct spinlock *);
extern void release(struct spinlock *lk);
/* 磁盘上的 inode 结构（存储在磁盘上的格式）*/

/* 内存中的 inode（缓存版，包含磁盘版本和额外运行时信息）*/

/* 目录项结构（目录文件中每一条记录的格式）*/

struct
{
  struct inode inode[NINODE]; // NINODE 在 param.h 中定义
} icache;

/* 声明外部函数（由其他模块提供）*/
extern struct buf *bread(uint dev, uint blockno);
extern void brelse(struct buf *b);
extern void bwrite(struct buf *b);
extern uint balloc(uint dev);

extern void *memcpy(void *dst, const void *src, unsigned long n);
extern void *memmove(void *dest, const void *src, unsigned long n);
extern void *memset(void *dst, int v, unsigned long n);
extern char *safestrcpy(char *s, const char *t, int n);
extern void iunlockput(struct inode *ip);
/* ================================================================
 * bmap — 将文件的逻辑块号映射到磁盘的物理块号
 *
 * 参数：
 *   ip — inode 指针
 *   bn — 文件内逻辑块编号（从 0 开始）
 *
 * 返回：对应的磁盘物理块号
 *
 * 映射规则（两层结构）：
 *   bn < NDIRECT     → 直接映射：ip->addrs[bn]
 *   bn < NINDIRECT   → 间接映射：读取间接块，在其中查找 addrs[bn-NDIRECT]
 *   否则             → 文件过大，panic
 *
 * 若目标块尚未分配（地址为0），自动调用 balloc() 分配新块。
 * ================================================================ */

uint bmap(struct inode *ip, uint bn)
{
  uint addr;
  struct buf *bp;
  uint *a;

  /* --- 1. 直接映射（前 NDIRECT 个逻辑块） --- */
  if (bn < NDIRECT)
  {
    if ((addr = ip->addrs[bn]) == 0)
    {
      /* [Lab7-任务2-步骤1]：分配一个新磁盘块并更新 inode 的地址数组 */
      addr = ip->addrs[bn] = balloc(ip->dev);
      // 注意：这里修改了 inode 内容，调用者通常会在随后调用 iupdate(ip) 将 inode 写回磁盘
    }
    return addr;
  }

  /* --- 逻辑块号减去直接块的数量，进入间接块范围 --- */
  bn -= NDIRECT;

  /* --- 2. 一级间接映射 --- */
  if (bn < NINDIRECT)
  {
    /* [步骤2]：确保“间接指针块”本身已经分配 */
    if ((addr = ip->addrs[NDIRECT]) == 0)
    {
      /* 分配一个块来存储这一堆物理块地址 */
      addr = ip->addrs[NDIRECT] = balloc(ip->dev);
    }

    /* [步骤3]：读取间接指针块 */
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data; // 将 buffer 的 1024 字节数据强制转换为 uint 数组

    /* 检查间接块中的第 bn 个条目是否已指向物理块 */
    if ((addr = a[bn]) == 0)
    {
      /* 分配实际的数据块 */
      addr = a[bn] = balloc(ip->dev);

      /* 重要：因为修改了索引块（buffer）的内容，必须刷回磁盘 */
      bwrite(bp);
    }

    /* 释放 buffer 占用，但不修改 valid，因为数据还在缓存里 */
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// 增加 inode 的引用计数
struct inode *idup(struct inode *ip)
{
  // acquire(&icache.lock);
  ip->ref++;
  // release(&icache.lock);
  return ip;
}
/* ================================================================
 * readi — 从 inode 文件中读取数据
 *
 * 参数：
 *   ip       — 要读取的 inode
 *   user_dst — 目标地址是否是用户虚拟地址（0=内核地址，1=用户地址）
 *   dst      — 目标缓冲区地址
 *   off      — 文件内偏移（字节）
 *   n        — 要读取的字节数
 *
 * 返回：实际读取的字节数（若 off 超过文件末尾则返回 0）
 * ================================================================ */
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off)
    return 0;
  if (off + n > ip->size)
    n = ip->size - off;

  for (tot = 0; tot < n; tot += m, off += m, dst += m)
  {
    /* 找到当前偏移所在的物理磁盘块 */
    uint blockno = bmap(ip, off / BSIZE);
    bp = bread(ip->dev, blockno);

    /* 计算本次从这一块可以读多少字节 */
    m = BSIZE - off % BSIZE;
    if (m > n - tot)
      m = n - tot;

    /* ================================================================
     * [Lab7-任务2-步骤4 修复]：
     * 根据 user_dst 决定是走上帝视角的 copyout，还是内核物理直写
     * ================================================================ */
    if (user_dst)
    {
      // 目标是用户态地址，调用查表搬运工，防止 scause=15 暴毙
      if (copyout(myproc()->pagetable, dst, (char *)(bp->data + off % BSIZE), m) < 0)
      {
        brelse(bp);
        break; // 拷贝失败（如越界或权限错误），释放磁盘块并提前结束
      }
    }
    else
    {
      // 目标是内核态地址（比如读文件系统元数据），直接搬运
      memmove((void *)dst, bp->data + off % BSIZE, m);
    }

    brelse(bp);
  }

  return (int)tot;
}

int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;
  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > (NDIRECT + NINDIRECT) * BSIZE)
    return -1;

  for (tot = 0; tot < n; tot += m, off += m, src += m)
  {
    uint blockno = bmap(ip, off / BSIZE);

    bp = bread(ip->dev, blockno);

    m = BSIZE - off % BSIZE;
    if (m > n - tot)
      m = n - tot;
    if (user_src)
    {
      memmove(bp->data + (off % BSIZE), (void *)src, m);
    }
    else
    {
      memmove(bp->data + (off % BSIZE), (void *)src, m);
    }
    bwrite(bp);
    brelse(bp);
  }

  if (n > 0 && off > ip->size)
  {
    ip->size = off;
    /* 如果文件大小改变了，必须同步 inode 结构体到磁盘 */
    iupdate(ip);
  }

  return n;
}

struct inode *iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  empty = 0;

  for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++)
  {
    /* 分支 A：匹配设备号和 inode 编号 (inum) */
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) // <--- 这里改为 ip->inum
    {
      ip->ref++;
      return ip;
    }

    if (empty == 0 && ip->ref == 0)
    {
      empty = ip;
    }
  }

  if (empty != 0)
  {
    ip = empty;
    ip->dev = dev;
    ip->inum = inum; // <--- 这里记录逻辑编号
    ip->ref = 1;
    ip->valid = 0; // 标记为无效，待 ilock 时根据图片中的公式去磁盘读取

    return ip;
  }

  panic("iget: no inodes");
}

void ilock(struct inode *ip)
{
  if (ip->valid == 0)
  {
    // 计算这个 inode 在哪个磁盘块
    uint blockno = sb.inodestart + ip->inum / IPB;

    struct buf *bp = bread(ip->dev, blockno);

    // 找到块内偏移处的 dinode
    // 明确字节偏移
    struct dinode *dip = (struct dinode *)(bp->data + (ip->inum % IPB) * sizeof(struct dinode));
    // 把磁盘 dinode 的字段复制到内存 inode
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;

    // // 1. 确认 blockno
    // printf("CRITICAL: Reading Inode %d at Block %d, IPB %d\n", ip->inum, blockno, IPB);

    // // 2. 直接看 bp->data 偏移 64 字节处（即 Inode 1）的原始 16 进制
    // unsigned char *raw = (unsigned char *)(bp->data + 64);
    // printf("RAW DATA at offset 64: %d %d %d %d\n", raw[0], raw[1], raw[2], raw[3]);

    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
  }
  if (ip->type == 0)
    panic("ilock: no type");
}

void iunlock(struct inode *ip)
{
  if (ip->ref >= 1)
    return;
  panic("iunlock");
}

void iput(struct inode *ip)
{
  ip->ref--;
  // 若引用归零且 nlink==0（文件已被 unlink），应当释放所有数据块
  // 简化版：先只做 ref 管理，不处理数据块释放
  if (ip->ref == 0)
    ip->valid = 0; // 标记缓存槽空闲，供下次 iget 重用
}

void iupdate(struct inode *ip)
{
  struct buf *bp = bread(ip->dev, sb.inodestart + ip->inum / IPB);
  struct dinode *dip = (struct dinode *)bp->data + ip->inum % IPB;
  // uint blockno = sb.inodestart + ip->inum / IPB;
  // if (ip->inum > 1)
  //   printf("DEBUG iupdate: writing inum %d to block %d\n", ip->inum, blockno);
  // 把内存字段写回磁盘 dinode（与 ilock 的方向相反）
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  bwrite(bp);
  brelse(bp);
}

struct inode *ialloc(uint dev, short type)
{
  for (int inum = 1; inum < sb.ninodes; inum++)
  {
    struct buf *bp = bread(dev, sb.inodestart + inum / IPB);
    struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
    if (dip->type == 0)
    { // 找到空闲 inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type; // 占用并设置类型
      bwrite(bp);
      brelse(bp);
      return iget(dev, inum); // 返回对应内存 inode
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}
/* ================================================================
 * dirlookup — 在目录 inode 中按文件名查找子条目
 *
 * 参数：
 *   dp   — 目录的 inode 指针（它的数据是一系列 struct dirent）
 *   name — 要查找的文件名
 *   poff — （可选输出）找到该条目在目录文件中的字节偏移
 *
 * 返回：找到则返回对应 inode 的指针（调用 iget）；未找到返回 0。
 *
 * 原理：目录也是文件！它的"文件内容"就是一条条 dirent 记录。
 * ================================================================ */
struct inode *dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off;
  struct dirent de;

  // 确保 dp 是个目录，否则 readi 会乱套
  if (dp->type != T_DIR)
    panic("dirlookup: not a directory");

  for (off = 0; off < dp->size; off += sizeof(de))
  {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup: read error");

    if (de.inum == 0)
      continue;

    // 比较文件名
    int match = 1;
    for (int i = 0; i < DIRSIZ; i++)
    {
      if (de.name[i] != name[i])
      {
        match = 0;
        break;
      }
      // 如果两个字符串都在此处结束，说明完全匹配
      if (de.name[i] == '\0')
        break;
    }

    if (match)
    {
      // 💡 关键修复：只有当 poff 不为 NULL 时才写入
      if (poff)
        *poff = off;

      return iget(dp->dev, de.inum);
    }
  }

  return 0; // 未找到
}

// 示例：skipelem("/a/b/c", name) -> 返回 "/b/c", name="a"
static char *skipelem(char *path, char *name)
{
  char *s;
  int len;

  // 1. 跳过开头的斜杠 '/'
  while (*path == '/')
    path++;

  // 2. 如果路径为空，返回 0
  if (*path == 0)
    return 0;

  // 3. 记录当前分量的开始位置，并寻找下一个斜杠或结尾
  s = path;
  while (*path != '/' && *path != 0)
    path++;

  // 4. 计算当前分量的长度
  len = path - s;

  // 5. 将分量拷贝到 name 中（最多 DIRSIZ 个字符）
  if (len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else
  {
    memmove(name, s, len);
    name[len] = 0; // 补上结束符
  }

  // 6. 继续跳过随后的斜杠
  while (*path == '/')
    path++;

  return path;
}

struct inode *namei(char *path)
{
  char name[DIRSIZ];
  struct inode *ip, *next;
  struct proc *p = myproc();
  /* 步骤 1: 决定起点 */
  // 如果以 '/' 开头，从根目录开始；否则从当前目录开始（此处简化始终从根开始）
  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
  {
    // 🚨 终极防弹衣：如果因为某些诡异原因当前进程没有 cwd，
    // 强制给它降级回绝对路径模式，绝不让它空指针崩溃！
    if (p->cwd == 0)
      ip = iget(ROOTDEV, ROOTINO);
    else
      ip = idup(p->cwd);
  }
  // ip = iget(ROOTDEV, ROOTINO); // 实验通常简化处理

  /* 步骤 2: 循环解析路径分量 */
  while ((path = skipelem(path, name)) != 0)
  {
    /* c. 准备读取目录内容，必须先加锁 */
    ilock(ip);

    /* d. 检查：只有目录才能进行 dirlookup */
    if (ip->type != T_DIR)
    {
      iunlockput(ip); // 解锁并释放引用
      return 0;
    }

    /* e. 在当前目录 ip 中查找名为 name 的条目 */
    if ((next = dirlookup(ip, name, 0)) == 0)
    {
      /* g. 文件不存在 */
      iunlockput(ip);
      return 0;
    }

    /* f. 已经找到了下一层，释放当前层的锁和引用 */
    iunlockput(ip);

    /* h. 将 ip 指向下一层，继续下一轮循环 */
    ip = next;
  }

  // 循环结束说明 path == 0，已到达终点
  return ip;
}

int dirlink(struct inode *dp, char *name, uint inum)
{
  uint off;
  struct dirent de;
  struct inode *ip;

  /* 0. 安全检查：确保该名字在目录中尚未存在 */
  if ((ip = dirlookup(dp, name, 0)) != 0)
  {
    iput(ip); // 如果找到了，说明重名，减少引用计数并报错
    return -1;
  }

  /* 1. & 2. 遍历目录，找一个空槽位 (de.inum == 0) */
  /* 注意：off 可能会增加到 dp->size，此时 writei 会自动扩展目录文件大小 */
  for (off = 0; off < dp->size; off += sizeof(de))
  {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink: readi");

    if (de.inum == 0) // 找到了一个被删除文件留下的空槽
      break;
  }

  /* 3. 填充 dirent 结构体 */
  de.inum = inum;

  // 关键错误预防：先用 memset 清零，防止残留垃圾数据影响以后 dirlookup 的比较
  memset(de.name, 0, sizeof(de.name));

  // 复制名字，注意不要超过 DIRSIZ
  safestrcpy(de.name, name, DIRSIZ);

  /* 4. 调用 writei() 将这条记录写回磁盘对应位置 */
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

void fsinit(int dev)
{
  struct buf *bp;
  binit();
  virtio_disk_init();
  bp = bread(dev, 1);
  memmove((void *)&sb, bp->data, sizeof(sb));
  brelse(bp);
  if (sb.magic != FSMAGIC)
    panic("invalid file system");

  for (int i = 0; i < NINODE; i++)
    icache.inode[i].ref = icache.inode[i].valid = 0;
}

void iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

struct inode *nameiparent(char *path, char *name)
{
  struct inode *ip, *next;
  struct proc *p = myproc();
  if (*path == '/')
  {
    ip = iget(ROOTDEV, ROOTINO);
  }
  else
  {
    if (p->cwd == 0)
      ip = iget(ROOTDEV, ROOTINO);
    else
      ip = idup(p->cwd);
  }

  while ((path = skipelem(path, name)) != 0)
  {
    ilock(ip);
    if (ip->type != T_DIR)
    {
      iunlockput(ip);
      return 0;
    }

    // 如果 path 为空，说明 name 里存的就是最后一层的名字，此时的 ip 就是父目录！
    if (*path == '\0')
    {
      iunlock(ip); // 注意：这里只解锁，不 put，因为要把父目录 inode 传给 sys_open 用
      return ip;
    }

    if ((next = dirlookup(ip, name, 0)) == 0)
    {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }

  if (ip)
    iput(ip);
  return 0;
}

void stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

int strncmp(const char *p, const char *q, uint n)
{
  while (n > 0 && *p && *p == *q)
  {
    n--;
    p++;
    q++;
  }
  if (n == 0)
    return 0;
  return (unsigned char)*p - (unsigned char)*q;
}

// 专门用于比对文件名的 namecmp
// DIRSIZ 通常在 fs.h 中定义，比如 #define DIRSIZ 14
int namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}