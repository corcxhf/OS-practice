#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

typedef long ssize_t;

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifdef __cplusplus
extern "C" {
#endif

int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
long lseek(int fd, long offset, int whence);
int ftruncate(int fd, long length);
int unlink(const char *pathname);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);
void *sbrk(long increment);
int isatty(int fd);
int dup(int oldfd);
int fork(void);
int execvp(const char *file, char *const argv[]);
int pipe(int pipefd[2]);
int waitpid(int pid, int *status, int options);
int kill(int pid, int sig);
int access(const char *path, int mode);

#ifdef __cplusplus
}
#endif

#define R_OK 4

#endif
