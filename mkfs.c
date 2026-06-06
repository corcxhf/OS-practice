#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#define BSIZE 1024
#define FSMAGIC 0x10203040
#define NDIRECT 12
#define DIRSIZ 14
// 1. 超级块结构
#define T_DIR 1    /* 目录文件 (Directory) */
#define T_FILE 2   /* 普通文件 (Regular file) */
#define T_DEVICE 3 /* 设备文件 (Device, 如串口或磁盘) */

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

struct dinode
{
    short type;              /* 文件类型（0=空闲, 1=普通文件, 2=目录, 3=设备）*/
    short major;             /* 设备主号（仅 type==3 时有效）*/
    short minor;             /* 设备次号（仅 type==3 时有效）*/
    short nlink;             /* 硬链接计数 */
    uint size;               /* 文件大小（字节数）*/
    uint addrs[NDIRECT + 1]; /* 数据块地址：前 NDIRECT 个是直接，最后一个是一级间接 */
};

struct dirent
{
    ushort inum;       /* 该条目对应的 inode 编号（0 表示空洞/已删除）*/
    char name[DIRSIZ]; /* 文件名（最多 14 字符，不含 '\0'）*/
};
int main(int argc, char *argv[])
{
    // 1. 接收命令行参数
    if (argc < 2)
    {
        printf("用法: mkfs fs.img [文件列表...]\n");
        return 1;
    }

    int fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        perror("创建 fs.img 失败");
        return 1;
    }

    char buf[BSIZE];
    memset(buf, 0, BSIZE);
    for (int i = 0; i < 2000; i++)
    {
        write(fd, buf, BSIZE);
    }

    // [2] 写入超级块
    struct superblock sb;
    sb.magic = FSMAGIC;
    sb.size = 2000;
    sb.nblocks = 1954;
    sb.ninodes = 200;
    sb.nlog = 30;
    sb.logstart = 2;
    sb.inodestart = 32;
    sb.bmapstart = 45;
    lseek(fd, 1 * BSIZE, SEEK_SET);
    write(fd, &sb, sizeof(sb));

    // 状态追踪变量
    uint freeinode = 2;  // Inode 1 是根目录，从 2 开始分给外部文件
    uint freeblock = 47; // 块 46 是根目录数据块，从 47 开始分给外部文件

    // 准备根目录的目录项数组 (假设最多打包 50 个文件)
    struct dirent de[50];
    memset(de, 0, sizeof(de));
    de[0].inum = 1;
    strcpy(de[0].name, ".");
    de[1].inum = 1;
    strcpy(de[1].name, "..");
    int de_count = 2;

    // [3] 循环处理 Makefile 传进来的每一个文件（比如 user/echo）
    for (int i = 2; i < argc; i++)
    {
        // 剥离 "user/" 前缀，只保留文件名（提取 "echo"）
        char *shortname = strrchr(argv[i], '/');
        if (shortname != NULL)
        {
            shortname++; // 跳过 '/'
        }
        else
        {
            shortname = argv[i];
        }

        // 打开外部待打包的文件
        int ffd = open(argv[i], O_RDONLY);
        if (ffd < 0)
        {
            printf("警告: 无法打开要打包的文件 %s\n", argv[i]);
            continue;
        }

        // 注册到根目录项
        de[de_count].inum = freeinode;
        strncpy(de[de_count].name, shortname, DIRSIZ);
        de_count++;

        // 初始化这个文件自己的 Inode
        struct dinode fin;
        memset(&fin, 0, sizeof(fin));
        fin.type = T_FILE; // T_FILE (普通文件)
        fin.nlink = 1;
        fin.size = 0;

        // 循环读取文件的真实数据，并写入数据块
        char fbuf[BSIZE];
        int cc;
        int blk_idx = 0;
        while ((cc = read(ffd, fbuf, BSIZE)) > 0)
        {
            if (blk_idx >= NDIRECT)
            {
                printf("错误: 文件 %s 太大了，超过了直接索引块数量\n", shortname);
                break;
            }
            // 把数据写进物理块
            lseek(fd, freeblock * BSIZE, SEEK_SET);
            write(fd, fbuf, BSIZE);

            // 记录到 Inode 中
            fin.addrs[blk_idx++] = freeblock;
            fin.size += cc;
            freeblock++; // 消耗了一个数据块
        }
        close(ffd);

        // 把这个文件的 Inode 写进磁盘的 Inode 区
        lseek(fd, 32 * BSIZE + freeinode * sizeof(struct dinode), SEEK_SET);
        write(fd, &fin, sizeof(fin));

        printf("成功打包: %s (Inode: %d, 大小: %d 字节)\n", shortname, freeinode, fin.size);
        freeinode++;
    }

    // [4] 写入根目录的 Inode
    struct dinode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.type = T_DIR; // T_DIR
    root_inode.nlink = 1;
    root_inode.size = de_count * sizeof(struct dirent); // 动态计算根目录大小
    root_inode.addrs[0] = 46;
    lseek(fd, 32 * BSIZE + 1 * sizeof(struct dinode), SEEK_SET);
    write(fd, &root_inode, sizeof(root_inode));

    // [5] 写入根目录的真实数据（目录项数组）到第 46 块
    lseek(fd, 46 * BSIZE, SEEK_SET);
    write(fd, de, root_inode.size);

    // [6] 绘制 Bitmap (根据真实消耗的 freeblock 来标记)
    unsigned char bmap[BSIZE];
    memset(bmap, 0, BSIZE);
    for (int i = 0; i < freeblock; i++)
    {
        bmap[i / 8] |= (1 << (i % 8));
    }
    lseek(fd, 45 * BSIZE, SEEK_SET);
    write(fd, bmap, BSIZE);

    close(fd);
    printf("====== 格式化完毕！======\n");
    printf("共打包了 %d 个外部程序。\n", de_count - 2);
    return 0;
}