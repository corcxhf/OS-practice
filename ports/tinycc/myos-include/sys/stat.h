#ifndef _SYS_STAT_H
#define _SYS_STAT_H

typedef long time_t;

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

#define R_OK 4

#ifdef __cplusplus
extern "C" {
#endif

int stat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);
int access(const char *path, int mode);

#ifdef __cplusplus
}
#endif

#endif
