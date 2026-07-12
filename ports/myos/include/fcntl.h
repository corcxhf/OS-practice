#ifndef _FCNTL_H
#define _FCNTL_H

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x200
#define O_TRUNC 0x400
#define O_EXCL 0x800
#define O_APPEND 0x1000
#define O_NONBLOCK 0x2000
#define O_ACCMODE 3
#define O_BINARY 0

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define FD_CLOEXEC 1
#define AT_FDCWD -100

#ifdef __cplusplus
extern "C" {
#endif

int open(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif
