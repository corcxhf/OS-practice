#ifndef _WCHAR_H
#define _WCHAR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

#ifndef __cplusplus
typedef int wchar_t;
#endif
typedef int wint_t;

typedef struct {
    int __state;
} mbstate_t;

size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
size_t wcstombs(char *dest, const wchar_t *src, size_t n);
size_t wcslen(const wchar_t *s);
int wcscoll(const wchar_t *a, const wchar_t *b);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
wint_t btowc(int c);
int wctob(wint_t c);

#ifdef __cplusplus
}
#endif

#endif
