#define ELF_MAGIC 0x464C457FU // "\x7FELF"
#include "types.h"
struct elfhdr
{
    uint32 magic;
    uchar elf[12];
    uint16 type;
    uint16 machine;
    uint32 version;
    uint64 entry;
    uint64 phoff;
    uint64 shoff;
    uint32 flags;
    uint16 ehsize;
    uint16 phentsize;
    uint16 phnum;
    uint16 shentsize;
    uint16 shnum;
    uint16 shstrndx;
};

struct proghdr
{
    uint32 type;
    uint32 flags; // 读/写/执行权限
    uint64 off;   // 该段在文件中的偏移量
    uint64 vaddr; // 该段在用户态运行时需要的虚拟地址
    uint64 paddr;
    uint64 filesz; // 该段在文件中的大小
    uint64 memsz;  // 该段在内存中的大小（memsz >= filesz，多出来的部分是 BSS 段，全填 0）
    uint64 align;
};
#define ELF_PROG_LOAD 1