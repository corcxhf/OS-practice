#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#ifdef __cplusplus
extern "C" {
#endif

int wait(int *status);
int waitpid(int pid, int *status, int options);

#ifdef __cplusplus
}
#endif

#define WNOHANG 1
#define WUNTRACED 2

#define WIFEXITED(status) (1)
#define WEXITSTATUS(status) (status)
#define WIFSIGNALED(status) (0)
#define WTERMSIG(status) (0)
#define WIFSTOPPED(status) (0)
#define WSTOPSIG(status) (0)

#endif
