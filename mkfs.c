#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef uint32_t uint;
typedef uint16_t ushort;

#define BSIZE 1024
#define FSMAGIC 0x10203040
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define DIRSIZ 14

#define T_DIR 1
#define T_FILE 2
#define T_DEVICE 3

#define FS_SIZE 2000
#define NINODES 200
#define NLOG 30
#define LOG_START 2
#define INODE_START 32
#define BMAP_START 45
#define ROOTINO 1
#define ROOT_BLOCK 46
#define FIRST_DATA_BLOCK 47

#define MAX_IMAGE_PATH 128
#define MAX_DIRS 32
#define MAX_DIRENTS (BSIZE / sizeof(struct dirent))

struct superblock
{
    uint magic;
    uint size;
    uint nblocks;
    uint ninodes;
    uint nlog;
    uint logstart;
    uint inodestart;
    uint bmapstart;
};

struct dinode
{
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

struct dirent
{
    ushort inum;
    char name[DIRSIZ];
};

struct image_dir
{
    char path[MAX_IMAGE_PATH];
    uint inum;
    uint parent_inum;
    uint blockno;
    struct dinode inode;
    struct dirent entries[MAX_DIRENTS];
    int nentry;
};

static int fsfd;
static uint freeinode = ROOTINO + 1;
static uint freeblock = FIRST_DATA_BLOCK;
static struct image_dir dirs[MAX_DIRS];
static int ndirs;

static struct image_dir *ensure_dir(const char *path);

static void die(const char *msg)
{
    fprintf(stderr, "mkfs: %s\n", msg);
    exit(1);
}

static void checked_lseek(off_t off)
{
    if (lseek(fsfd, off, SEEK_SET) != off)
        die("lseek failed");
}

static void checked_write(const void *buf, size_t n)
{
    if (write(fsfd, buf, n) != (ssize_t)n)
        die("write failed");
}

static void write_block(uint blockno, const void *buf)
{
    checked_lseek((off_t)blockno * BSIZE);
    checked_write(buf, BSIZE);
}

static void zero_block(uint blockno)
{
    char zero[BSIZE];
    memset(zero, 0, sizeof(zero));
    write_block(blockno, zero);
}

static uint alloc_inode(void)
{
    if (freeinode >= NINODES)
        die("out of inodes");
    return freeinode++;
}

static uint alloc_block(void)
{
    if (freeblock >= FS_SIZE)
        die("out of blocks");
    zero_block(freeblock);
    return freeblock++;
}

static void write_inode(uint inum, const struct dinode *inode)
{
    checked_lseek((off_t)INODE_START * BSIZE + inum * sizeof(*inode));
    checked_write(inode, sizeof(*inode));
}

static void init_fs_image(void)
{
    char zero[BSIZE];
    struct superblock sb;

    memset(zero, 0, sizeof(zero));
    for (int i = 0; i < FS_SIZE; i++)
        checked_write(zero, sizeof(zero));

    memset(&sb, 0, sizeof(sb));
    sb.magic = FSMAGIC;
    sb.size = FS_SIZE;
    sb.nblocks = FS_SIZE - FIRST_DATA_BLOCK + 1;
    sb.ninodes = NINODES;
    sb.nlog = NLOG;
    sb.logstart = LOG_START;
    sb.inodestart = INODE_START;
    sb.bmapstart = BMAP_START;

    checked_lseek((off_t)BSIZE);
    checked_write(&sb, sizeof(sb));
}

static void check_component_name(const char *name)
{
    int len = strlen(name);
    if (len == 0)
        die("empty path component");
    if (len >= DIRSIZ)
        die("path component is too long for DIRSIZ");
}

static void dir_add(struct image_dir *dir, const char *name, uint inum)
{
    if (dir->nentry >= MAX_DIRENTS)
        die("directory has too many entries");

    for (int i = 0; i < dir->nentry; i++)
    {
        if (strncmp(dir->entries[i].name, name, DIRSIZ) == 0)
            die("duplicate directory entry");
    }

    dir->entries[dir->nentry].inum = inum;
    memset(dir->entries[dir->nentry].name, 0, DIRSIZ);
    strncpy(dir->entries[dir->nentry].name, name, DIRSIZ);
    dir->nentry++;
}

static struct image_dir *find_dir(const char *path)
{
    for (int i = 0; i < ndirs; i++)
    {
        if (strcmp(dirs[i].path, path) == 0)
            return &dirs[i];
    }
    return 0;
}

static struct image_dir *new_dir(const char *path, uint parent_inum)
{
    struct image_dir *dir;

    if (ndirs >= MAX_DIRS)
        die("too many directories");

    dir = &dirs[ndirs++];
    memset(dir, 0, sizeof(*dir));
    strncpy(dir->path, path, sizeof(dir->path) - 1);
    dir->inum = ndirs == 1 ? ROOTINO : alloc_inode();
    dir->parent_inum = parent_inum;
    dir->blockno = dir->inum == ROOTINO ? ROOT_BLOCK : alloc_block();
    dir->inode.type = T_DIR;
    dir->inode.nlink = 2;
    dir->inode.addrs[0] = dir->blockno;

    dir_add(dir, ".", dir->inum);
    dir_add(dir, "..", parent_inum);
    return dir;
}

static void init_root_dir(void)
{
    zero_block(ROOT_BLOCK);
    new_dir("/", ROOTINO);
}

static void init_standard_dirs(void)
{
    ensure_dir("/bin");
    ensure_dir("/lib");
}

static void parent_and_name(const char *path, char *parent, char *name)
{
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    size_t base_len = strlen(base);

    if (base_len == 0 || base_len >= DIRSIZ)
        die("path component is too long for DIRSIZ");

    if (slash == 0 || slash == path)
    {
        strcpy(parent, "/");
        strcpy(name, base);
    }
    else
    {
        size_t len = slash - path;
        if (len >= MAX_IMAGE_PATH)
            die("parent path is too long");
        memcpy(parent, path, len);
        parent[len] = '\0';
        strcpy(name, base);
    }
}

static void append_component(char *path, const char *name)
{
    int len = strlen(path);
    if (strcmp(path, "/") != 0)
        path[len++] = '/';

    if (len + (int)strlen(name) >= MAX_IMAGE_PATH)
        die("image path is too long");
    strcpy(path + len, name);
}

static struct image_dir *ensure_dir(const char *path)
{
    char current[MAX_IMAGE_PATH];
    char component[DIRSIZ + 1];
    const char *p = path;
    struct image_dir *dir;

    if (path[0] != '/')
        die("internal error: non-absolute directory path");

    dir = find_dir("/");
    if (strcmp(path, "/") == 0)
        return dir;

    strcpy(current, "/");
    while (*p == '/')
        p++;

    while (*p)
    {
        int n = 0;
        while (p[n] && p[n] != '/')
            n++;
        if (n >= DIRSIZ)
            die("path component is too long for DIRSIZ");

        memcpy(component, p, n);
        component[n] = '\0';
        check_component_name(component);
        append_component(current, component);

        struct image_dir *next = find_dir(current);
        if (next == 0)
        {
            next = new_dir(current, dir->inum);
            dir_add(dir, component, next->inum);
            dir->inode.nlink++;
        }
        dir = next;

        p += n;
        while (*p == '/')
            p++;
    }

    return dir;
}

static void normalize_dest_path(const char *input, char *out)
{
    int n = 0;

    if (input[0] != '/')
        out[n++] = '/';

    for (int i = 0; input[i] && n < MAX_IMAGE_PATH - 1; i++)
    {
        if (input[i] == '/' && n > 1 && out[n - 1] == '/')
            continue;
        out[n++] = input[i];
    }

    while (n > 1 && out[n - 1] == '/')
        n--;

    out[n] = '\0';
    if (n == 0 || strcmp(out, "/") == 0)
        die("destination path must name a file");
}

static const char *host_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void parse_mapping(const char *arg, char *src, char *dst)
{
    const char *eq = strchr(arg, '=');

    if (eq)
    {
        size_t n = eq - arg;
        if (n == 0 || n >= MAX_IMAGE_PATH)
            die("invalid source path in mapping");
        memcpy(src, arg, n);
        src[n] = '\0';
        normalize_dest_path(eq + 1, dst);
    }
    else
    {
        if (strlen(arg) >= MAX_IMAGE_PATH)
            die("source path is too long");
        strcpy(src, arg);
        snprintf(dst, MAX_IMAGE_PATH, "/%s", host_basename(arg));
    }
}

static void add_file_data(int hostfd, struct dinode *inode, const char *src)
{
    uint indirect[NINDIRECT];
    char buf[BSIZE];
    int block_index = 0;
    ssize_t nread;

    memset(indirect, 0, sizeof(indirect));
    while (memset(buf, 0, sizeof(buf)), (nread = read(hostfd, buf, sizeof(buf))) > 0)
    {
        uint data_block;

        if (block_index >= NDIRECT + NINDIRECT)
        {
            fprintf(stderr, "mkfs: file too large: %s\n", src);
            exit(1);
        }

        if (block_index == NDIRECT)
            inode->addrs[NDIRECT] = alloc_block();

        data_block = alloc_block();
        write_block(data_block, buf);

        if (block_index < NDIRECT)
            inode->addrs[block_index] = data_block;
        else
            indirect[block_index - NDIRECT] = data_block;

        inode->size += nread;
        block_index++;
    }

    if (nread < 0)
    {
        fprintf(stderr, "mkfs: read failed: %s\n", src);
        exit(1);
    }

    if (inode->addrs[NDIRECT] != 0)
        write_block(inode->addrs[NDIRECT], indirect);
}

static void add_file(const char *src, const char *dst)
{
    char parent[MAX_IMAGE_PATH];
    char name[DIRSIZ + 1];
    struct image_dir *dir;
    struct dinode inode;
    uint inum;
    int hostfd;

    parent_and_name(dst, parent, name);
    check_component_name(name);
    dir = ensure_dir(parent);

    hostfd = open(src, O_RDONLY);
    if (hostfd < 0)
    {
        fprintf(stderr, "mkfs: cannot open %s\n", src);
        exit(1);
    }

    inum = alloc_inode();
    memset(&inode, 0, sizeof(inode));
    inode.type = T_FILE;
    inode.nlink = 1;
    add_file_data(hostfd, &inode, src);
    close(hostfd);

    write_inode(inum, &inode);
    dir_add(dir, name, inum);
    printf("成功打包: %s <= %s (Inode: %u, 大小: %u 字节)\n", dst, src, inum, inode.size);
}

static void flush_dirs(void)
{
    for (int i = 0; i < ndirs; i++)
    {
        char block[BSIZE];
        struct image_dir *dir = &dirs[i];

        memset(block, 0, sizeof(block));
        dir->inode.size = dir->nentry * sizeof(struct dirent);
        memcpy(block, dir->entries, dir->inode.size);
        write_block(dir->blockno, block);
        write_inode(dir->inum, &dir->inode);
    }
}

static void write_bitmap(void)
{
    unsigned char bmap[BSIZE];

    memset(bmap, 0, sizeof(bmap));
    for (uint i = 0; i < freeblock; i++)
        bmap[i / 8] |= 1 << (i % 8);
    write_block(BMAP_START, bmap);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: mkfs fs.img [hostfile[=imagepath] ...]\n");
        return 1;
    }

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0)
    {
        perror("mkfs");
        return 1;
    }

    init_fs_image();
    init_root_dir();
    init_standard_dirs();

    for (int i = 2; i < argc; i++)
    {
        char src[MAX_IMAGE_PATH];
        char dst[MAX_IMAGE_PATH];

        parse_mapping(argv[i], src, dst);
        add_file(src, dst);
    }

    flush_dirs();
    write_bitmap();
    close(fsfd);

    printf("====== 格式化完毕！======\n");
    printf("共打包了 %d 个外部文件，%d 个目录。\n", argc - 2, ndirs);
    return 0;
}
