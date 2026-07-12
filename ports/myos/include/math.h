#ifndef _MATH_H
#define _MATH_H

#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inf())
#define NAN (__builtin_nan(""))

#ifdef __cplusplus
extern "C" {
#endif

int isnan(double x);
int isinf(double x);
int finite(double x);
double fabs(double x);
double frexp(double x, int *exp);
double ldexp(double x, int exp);

#ifdef __cplusplus
}
#endif

#endif
