#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

#include <sys/time.h>

typedef unsigned long rlim_t;

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN -1
#define RLIM_INFINITY ((rlim_t)-1)
#define RLIMIT_STACK 3
#define RLIMIT_NOFILE 7

#ifdef __cplusplus
extern "C" {
#endif

int getrusage(int who, struct rusage *usage);
int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);

#ifdef __cplusplus
}
#endif

#endif
