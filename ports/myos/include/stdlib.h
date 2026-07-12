#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767
#define MB_CUR_MAX 1

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
long atol(const char *s);
long long atoll(const char *s);
int abs(int x);
long labs(long x);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
float strtof(const char *nptr, char **endptr);
long double strtold(const char *nptr, char **endptr);
double atof(const char *nptr);
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
int rand(void);
void srand(unsigned int seed);

extern char **environ;

#ifdef __cplusplus
}
#endif

#endif
