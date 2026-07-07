#ifndef _TIME_H
#define _TIME_H

typedef long time_t;

#ifdef __cplusplus
extern "C" {
#endif

time_t time(time_t *tloc);

#ifdef __cplusplus
}
#endif

#endif
