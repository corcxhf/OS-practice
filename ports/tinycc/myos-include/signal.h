#ifndef _SIGNAL_H
#define _SIGNAL_H

#define SIGWINCH 28
#define SIGINT 2
#define SIGSTOP 19
#define SIG_IGN ((sighandler_t)1)
#define SIG_DFL ((sighandler_t)0)

typedef void (*sighandler_t)(int);

#ifdef __cplusplus
extern "C" {
#endif

sighandler_t signal(int signum, sighandler_t handler);

#ifdef __cplusplus
}
#endif

#endif
