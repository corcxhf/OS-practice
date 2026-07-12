#ifndef _FCNTL_H
#define _FCNTL_H

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x200
#define O_TRUNC 0x400
#define O_NONBLOCK 0x800

#define F_GETFL 3
#define F_SETFL 4

#ifdef __cplusplus
extern "C" {
#endif

int open(const char *pathname, int flags, ...);
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif
