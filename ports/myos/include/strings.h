#ifndef _STRINGS_H
#define _STRINGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int bcmp(const void *a, const void *b, size_t n);
void bcopy(const void *src, void *dst, size_t n);
void bzero(void *s, size_t n);
int ffs(int i);
char *index(const char *s, int c);
char *rindex(const char *s, int c);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);

#ifdef __cplusplus
}
#endif

#endif
