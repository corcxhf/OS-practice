#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

#define FSMAGIC 0x10203040
#define ROOTINO 1
#define T_DIR 1    /* 目录文件 (Directory) */
#define T_FILE 2   /* 普通文件 (Regular file) */
#define T_DEVICE 3 /* 设备文件 (Device, 如串口或磁盘) */

#define FD_NONE 0
#define FD_PIPE 1
#define FD_INODE 2
#define FD_DEVICE 3

struct superblock
{
    uint magic;      // 魔数，文件系统类型验证码（必须 == FSMAGIC，验证这确实是
    uint size;       // 磁盘总块数
    uint nblocks;    // 数据块总数
    uint ninodes;    // 最大 inode 数
    uint nlog;       // 日志区块数
    uint logstart;   // 日志区起始块号
    uint inodestart; // inode 区起始块号
    uint bmapstart;  // 位图区起始块号
};

extern struct superblock sb;

struct file
{
    int type;         // FD_NONE=0, FD_INODE=1（本实验先只支持 inode 文件）
    int ref;          // 引用计数（fork 后父子共享时 ref==2）
    char readable;    // 是否可读
    char writable;    // 是否可写
    struct inode *ip; // 对应的 inode（type==FD_INODE 时有效）
    uint off;         // 当前读写偏移（字节）
}; // 全局文件表（系统中所有打开的文件对象）

struct buf
{
    int valid;         /* 当前缓存的数据是否有效（从磁盘读取过）*/
    int disk;          /* 是否正在与磁盘驱动交互（等待读写完成）*/
    uint dev;          /* 设备号 */
    uint blockno;      /* 磁盘块号 */
    uint refcnt;       /* 引用计数（有多少人在使用这块缓冲）*/
    struct buf *prev;  /* LRU 链表前驱 */
    struct buf *next;  /* LRU 链表后继 */
    uchar data[BSIZE]; /* 磁盘块的实际数据（1024字节）*/
};

struct inode
{
    uint dev;  /* 设备号 */
    uint inum; /* inode 编号 */
    int ref;   /* 引用计数 */
    int valid; /* 内容是否从磁盘读入 */
    /* 以下字段来自磁盘 dinode（读入后缓存在这里）*/
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

struct dinode
{
    short type;              /* 文件类型（0=空闲, 1=普通文件, 2=目录, 3=设备）*/
    short major;             /* 设备主号（仅 type==3 时有效）*/
    short minor;             /* 设备次号（仅 type==3 时有效）*/
    short nlink;             /* 硬链接计数 */
    uint size;               /* 文件大小（字节数）*/
    uint addrs[NDIRECT + 1]; /* 数据块地址：前 NDIRECT 个是直接，最后一个是一级间接 */
};