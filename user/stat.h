// user/stat.h
#ifndef STAT_H
#define STAT_H

#include "fs.h"
#include "types.h"

struct stat
{
    int type;    // 文件类型 (T_DIR, T_FILE, etc.)
    int dev;     // 文件系统盘号
    uint64 ino;  // Inode 编号
    short nlink; // 硬链接数
    uint64 size; // 🌟 文件大小（字节数）- 务必确认内核里也是 uint64/long!
};

#endif