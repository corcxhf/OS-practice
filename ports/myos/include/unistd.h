#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <sys/types.h>

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
int rmdir(const char *pathname);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);
void *sbrk(long increment);
void _exit(int status);
unsigned int sleep(unsigned int seconds);
int isatty(int fd);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int fork(void);
int getpid(void);
uid_t getuid(void);
gid_t getgid(void);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int pipe(int pipefd[2]);
int wait(int *status);
int waitpid(int pid, int *status, int options);
int kill(int pid, int sig);
int access(const char *path, int mode);
int faccessat(int dirfd, const char *pathname, int mode, int flags);
int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);
int readlink(const char *path, char *buf, size_t bufsiz);
long sysconf(int name);
int getpagesize(void);

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char *const argv[], const char *optstring);

#ifdef __cplusplus
}
#endif

#define AT_FDCWD -100
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _PC_PATH_MAX 4

long pathconf(const char *path, int name);

#endif
