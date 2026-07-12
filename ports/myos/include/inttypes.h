#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>

#define PRId8 "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "ld"
#define PRIi8 "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 "li"
#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "lu"
#define PRIx8 "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "lx"
#define PRIX8 "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 "lX"
#define PRIo8 "o"
#define PRIo16 "o"
#define PRIo32 "o"
#define PRIo64 "lo"

#define PRIdPTR "ld"
#define PRIiPTR "li"
#define PRIuPTR "lu"
#define PRIxPTR "lx"
#define PRIXPTR "lX"
#define PRIoPTR "lo"

#define PRIdMAX "ld"
#define PRIiMAX "li"
#define PRIuMAX "lu"
#define PRIxMAX "lx"
#define PRIXMAX "lX"
#define PRIoMAX "lo"

#define SCNd8 "d"
#define SCNd16 "d"
#define SCNd32 "d"
#define SCNd64 "ld"
#define SCNi8 "i"
#define SCNi16 "i"
#define SCNi32 "i"
#define SCNi64 "li"
#define SCNu8 "u"
#define SCNu16 "u"
#define SCNu32 "u"
#define SCNu64 "lu"
#define SCNx8 "x"
#define SCNx16 "x"
#define SCNx32 "x"
#define SCNx64 "lx"
#define SCNo8 "o"
#define SCNo16 "o"
#define SCNo32 "o"
#define SCNo64 "lo"

#define SCNdPTR "ld"
#define SCNiPTR "li"
#define SCNuPTR "lu"
#define SCNxPTR "lx"
#define SCNoPTR "lo"

#define SCNdMAX "ld"
#define SCNiMAX "li"
#define SCNuMAX "lu"
#define SCNxMAX "lx"
#define SCNoMAX "lo"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    intmax_t quot;
    intmax_t rem;
} imaxdiv_t;

intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);

#ifdef __cplusplus
}
#endif

#endif
