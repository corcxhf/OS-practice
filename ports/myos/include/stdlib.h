#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void exit(int status);
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void abort(void);
int atoi(const char *s);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
int mkstemp(char *template);
int mkstemps(char *template, int suffixlen);
char *mkdtemp(char *template);
char *realpath(const char *path, char *resolved_path);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
int atexit(void (*function)(void));
long sysconf(int name);
int getpagesize(void);

extern char **environ;

#ifdef __cplusplus
}
#endif

#endif
