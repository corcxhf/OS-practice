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
int truncate(const char *path, long length);
int unlink(const char *pathname);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);
void *sbrk(long increment);
int isatty(int fd);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int fork(void);
int getpid(void);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int pipe(int pipefd[2]);
int waitpid(int pid, int *status, int options);
int kill(int pid, int sig);
int access(const char *path, int mode);
int faccessat(int dirfd, const char *pathname, int mode, int flags);

#ifdef __cplusplus
}
#endif

#define AT_FDCWD -100
#define R_OK 4

#endif
