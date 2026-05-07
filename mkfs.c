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

int main()
{
    // 创建一个全新的 fs.img
    int fd = open("fs.img", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        perror("创建 fs.img 失败");
        return 1;
    }

    char buf[BSIZE];
    memset(buf, 0, BSIZE);

    // [1] 填充满 2000 个全 0 的扇区 (2000 * 1024 = 2MB 大小的硬盘)
    for (int i = 0; i < 2000; i++)
    {
        write(fd, buf, BSIZE);
    }

    // [2] 写入超级块 (Superblock) 到第 1 块
    struct superblock sb;
    sb.magic = FSMAGIC;
    sb.size = 2000;     // 总块数
    sb.nblocks = 1954;  // 数据块数
    sb.ninodes = 200;   // 支持 200 个文件
    sb.nlog = 30;       // 30 个日志块
    sb.logstart = 2;    // 日志区域起始于第 2 块
    sb.inodestart = 32; // Inode 区域起始于第 32 块
    sb.bmapstart = 45;  // Bitmap 区域起始于第 45 块

    lseek(fd, 1 * BSIZE, SEEK_SET);
    write(fd, &sb, sizeof(sb));

    // [3] 注入根目录 Inode (编号必须是 1)
    struct dinode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.type = 1; // T_DIR = 1 (这是一个目录)
    root_inode.nlink = 1;
    root_inode.size = 2 * sizeof(struct dirent); // 里面只装了 "." 和 ".."
    root_inode.addrs[0] = 46;                    // 这个目录的数据存在第 46 块！

    // 定位到 Inode 1 的位置 (跳过 Inode 0)
    lseek(fd, 32 * BSIZE + 1 * sizeof(struct dinode), SEEK_SET);
    write(fd, &root_inode, sizeof(root_inode));

    // [4] 写入根目录的真实内容 (创建 "." 和 ".." 映射)
    struct dirent de[2];
    memset(de, 0, sizeof(de));
    de[0].inum = 1;
    strcpy(de[0].name, ".");
    de[1].inum = 1;
    strcpy(de[1].name, "..");

    // 写入第 46 块
    lseek(fd, 46 * BSIZE, SEEK_SET);
    write(fd, de, sizeof(de));

    // [5] 绘制 Bitmap (标记哪些块已经被用掉了)
    // 我们用掉了块 0 到块 46，共 47 个块。在 Bitmap 中把前 47 个 bit 标为 1
    unsigned char bmap[BSIZE];
    memset(bmap, 0, BSIZE);
    for (int i = 0; i <= 46; i++)
    {
        bmap[i / 8] |= (1 << (i % 8));
    }

    lseek(fd, 45 * BSIZE, SEEK_SET);
    write(fd, bmap, BSIZE);

    close(fd);
    printf("====== 格式化完毕！======\n");
    printf("超级块、根目录 '/' 和 Bitmap 已成功注入 fs.img！\n");
    return 0;
}