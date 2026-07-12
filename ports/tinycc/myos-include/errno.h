#ifndef _ERRNO_H
#define _ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

extern int *__errno_location(void);

#ifdef __cplusplus
}
#endif

#define errno (*__errno_location())

#define EPERM 1
#define ENOENT 2
#define EIO 5
#define EAGAIN 11
#define ENOMEM 12
#define EINVAL 22
#define ENOTTY 25

#endif
