#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

struct stat {
    int st_dev;
    unsigned int st_ino;
    short st_mode;
    short st_nlink;
    long st_size;
    time_t st_mtime;
};

#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IFMT 0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_ISDIR(m) ((m) == 1 || (((m) & S_IFMT) == S_IFDIR))
#define S_ISREG(m) ((m) == 2 || (((m) & S_IFMT) == S_IFREG))

#define R_OK 4
#define AT_FDCWD -100

#ifdef __cplusplus
extern "C" {
#endif

int stat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);
int fstatat(int dirfd, const char *pathname, struct stat *st, int flags);
int access(const char *path, int mode);
int mkdir(const char *path, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
