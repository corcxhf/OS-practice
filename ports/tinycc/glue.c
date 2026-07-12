// tinycc/glue.c
#include "types.h"
#include "fs.h"
#include <stdarg.h> // 编译器自带的变长参数头文件，nostdlib 下也可以安全引用
#include "proc.h"
// #define SYS_exit 2
// #define SYS_open 15
// #define SYS_write 16
// #define SYS_read 17
// #define SYS_close 18
// #define SYS_fstat 19
// #define SYS_unlink 24

// 祖传无敌系统调用内联汇编
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm("a0") = a0;
    register uint64 a1_asm __asm("a1") = a1;
    register uint64 a2_asm __asm("a2") = a2;
    register uint64 a7_asm __asm("a7") = n;
    __asm__ volatile("ecall" : "+r"(a0_asm) : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm) : "memory");
    return a0_asm;
}

// =========================================================================
// 1. 基础内存与原生系统调用函数 (直接对齐 GCC 内置签名)
// =========================================================================
void *memset(void *dst, int c, uint64 n)
{
    char *cdst = (char *)dst;
    for (uint64 i = 0; i < n; i++)
        cdst[i] = c;
    return dst;
}

void *memcpy(void *dst, const void *src, uint64 n)
{
    char *cdst = (char *)dst;
    const char *csrc = (const char *)src;
    for (uint64 i = 0; i < n; i++)
        cdst[i] = csrc[i];
    return dst;
}

void *memmove(void *dst, const void *src, uint64 n)
{
    char *d = dst;
    const char *s = src;
    if (d < s)
    {
        while (n-- > 0)
            *d++ = *s++;
    }
    else
    {
        d += n;
        s += n;
        while (n-- > 0)
            *--d = *--s;
    }
    return dst;
}
int open(const char *pathname, int flags, ...)
{
    int myos_flags = flags & 0x3;
    if (flags & 64)
        myos_flags |= 0x200;
    if (flags & 512)
        myos_flags |= 0x400;

    return syscall(SYS_open, (uint64)pathname, myos_flags, 0);
}

int close(int fd)
{
    return (int)syscall(SYS_close, (uint64)fd, 0, 0);
}

int read(int fd, void *buf, uint64 n)
{
    return (int)syscall(SYS_read, (uint64)fd, (uint64)buf, n);
}

long write(int fd, const void *buf, unsigned long count)
{
    return syscall(SYS_write, fd, (uint64)buf, count);
}

long lseek(int fd, long offset, int whence)
{
    return syscall(SYS_lseek, fd, offset, whence);
}

// =========================================================================
// 2. 极其硬核的 🌟微型格式化打印引擎🌟 (爆破 printf/fprintf/snprintf)
// =========================================================================
typedef struct
{
    int fd;
} FILE;
FILE _stubs[3] = {{0}, {1}, {2}};
FILE *stdin = &_stubs[0];
FILE *stdout = &_stubs[1];
FILE *stderr = &_stubs[2];

int vsnprintf(char *buf, uint64 size, const char *fmt, va_list ap)
{
    uint64 i = 0;
    const char *p = fmt;
    if (!buf || size == 0)
        return 0;
    while (*p && i < size - 1)
    {
        if (*p == '%')
        {
            int precision = -1;
            int long_count = 0;
            p++;

            while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0')
                p++;
            if (*p == '*')
            {
                (void)va_arg(ap, int);
                p++;
            }
            else
            {
                while (*p >= '0' && *p <= '9')
                    p++;
            }
            if (*p == '.')
            {
                p++;
                if (*p == '*')
                {
                    precision = va_arg(ap, int);
                    p++;
                }
                else
                {
                    precision = 0;
                    while (*p >= '0' && *p <= '9')
                    {
                        precision = precision * 10 + (*p - '0');
                        p++;
                    }
                }
            }
            while (*p == 'l' || *p == 'h' || *p == 'z' || *p == 't' || *p == 'j')
            {
                if (*p == 'l')
                    long_count++;
                else if (*p == 'z' || *p == 't' || *p == 'j')
                    long_count = 1;
                p++;
            }
            if (*p == 's')
            {
                char *s = va_arg(ap, char *);
                int written = 0;
                if (!s)
                    s = "(null)";
                while (*s && i < size - 1 && (precision < 0 || written < precision))
                {
                    buf[i++] = *s++;
                    written++;
                }
            }
            else if (*p == 'd' || *p == 'i')
            {
                long long val = long_count ? va_arg(ap, long) : va_arg(ap, int);
                unsigned long long uval;
                if (val < 0)
                {
                    buf[i++] = '-';
                    uval = (unsigned long long)(-val);
                }
                else
                {
                    uval = (unsigned long long)val;
                }
                char tmp[32];
                int tp = 0;
                if (uval == 0)
                    tmp[tp++] = '0';
                while (uval > 0)
                {
                    tmp[tp++] = '0' + (uval % 10);
                    uval /= 10;
                }
                while (tp > 0 && i < size - 1)
                    buf[i++] = tmp[--tp];
            }
            else if (*p == 'u')
            {
                unsigned long long val = long_count ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                char tmp[32];
                int tp = 0;
                if (val == 0)
                    tmp[tp++] = '0';
                while (val > 0)
                {
                    tmp[tp++] = '0' + (val % 10);
                    val /= 10;
                }
                while (tp > 0 && i < size - 1)
                    buf[i++] = tmp[--tp];
            }
            else if (*p == 'x' || *p == 'p' || *p == 'X')
            {
                unsigned long long val = (*p == 'p' || long_count) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                char tmp[32];
                int tp = 0;
                if (val == 0)
                    tmp[tp++] = '0';
                while (val > 0)
                {
                    int digit = val % 16;
                    tmp[tp++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                    val /= 16;
                }
                while (tp > 0 && i < size - 1)
                    buf[i++] = tmp[--tp];
            }
            else if (*p == 'c')
            {
                buf[i++] = (char)va_arg(ap, int);
            }
            else if (*p == '%')
            {
                buf[i++] = '%';
            }
            else
            {
                if (i < size - 1)
                    buf[i++] = *p;
            }
        }
        else
        {
            buf[i++] = *p;
        }
        p++;
    }
    buf[i] = '\0';
    return i;
}

int snprintf(char *buf, uint64 size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, 4096, fmt, ap);
    va_end(ap);
    return ret;
}

int vsnprintf_chk(char *buf, uint64 size, int flags, uint64 os, const char *fmt, va_list ap)
{
    return vsnprintf(buf, size, fmt, ap);
}

int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    char buf[512];
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    syscall(SYS_write, (uint64)stream->fd, (uint64)buf, ret);
    return ret;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(stream, fmt, ap);
    va_end(ap);
    return ret;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    syscall(SYS_write, 1, (uint64)buf, ret);
    va_end(ap);
    return ret;
}

// =========================================================================
// 3. 虚拟高级文件流与字符串适配器
// =========================================================================
FILE *fopen(const char *filename, const char *mode)
{
    int flags = 0;
    if (mode[0] == 'w')
        flags = 1 | 64 | 512;
    int fd = open(filename, flags);
    if (fd < 0)
        return 0;
    extern void *malloc(uint64);
    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (f == 0)
    {
        close(fd);
        return 0;
    }
    f->fd = fd;
    return f;
}

int fclose(FILE *stream)
{
    int ret = syscall(SYS_close, (uint64)stream->fd, 0, 0);
    extern void free(void *);
    if (stream != stdin && stream != stdout && stream != stderr)
        free(stream);
    return ret;
}

uint64 fread(void *ptr, uint64 size, uint64 nmemb, FILE *stream)
{
    int ret = syscall(SYS_read, (uint64)stream->fd, (uint64)ptr, size * nmemb);
    return ret < 0 ? 0 : ret / size;
}

uint64 fwrite(const void *ptr, uint64 size, uint64 nmemb, FILE *stream)
{
    int ret = syscall(SYS_write, (uint64)stream->fd, (uint64)ptr, size * nmemb);
    return ret < 0 ? 0 : ret / size;
}

int fputs(const char *s, FILE *stream)
{
    extern uint64 strlen(const char *);
    return syscall(SYS_write, (uint64)stream->fd, (uint64)s, strlen(s));
}

int fputc(int c, FILE *stream)
{
    char ch = (char)c;
    return syscall(SYS_write, (uint64)stream->fd, (uint64)&ch, 1) == 1 ? c : -1;
}

int fgetc(FILE *stream)
{
    char ch;
    if (syscall(SYS_read, (uint64)stream->fd, (uint64)&ch, 1) == 1)
        return (int)ch;
    return -1;
}

int fflush(FILE *stream) { return 0; }
int fseek(FILE *stream, long offset, int whence)
{
    return lseek(stream->fd, offset, whence) < 0 ? -1 : 0;
}
long ftell(FILE *stream) { return lseek(stream->fd, 0, SEEK_CUR); }
FILE *fdopen(int fd, const char *mode)
{
    (void)mode;
    if (fd < 0)
        return 0;
    extern void *malloc(uint64);
    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (f == 0)
        return 0;
    f->fd = fd;
    return f;
}
FILE *freopen(const char *path, const char *mode, FILE *stream) { return stream; }

uint64 strlen(const char *s)
{
    uint64 len = 0;
    while (s[len])
        len++;
    return len;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++))
        ;
    return ret;
}

char *strncpy(char *dst, const char *src, uint64 n)
{
    char *ret = dst;
    while (n > 0 && (*dst++ = *src++))
        n--;
    while (n > 0)
    {
        *dst++ = 0;
        n--;
    }
    return ret;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, uint64 n)
{
    while (n > 0 && *s1 && *s1 == *s2)
    {
        n--;
        s1++;
        s2++;
    }
    if (n == 0)
        return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    if (c == 0)
        return (char *)s;
    return 0;
}

char *strrchr(const char *s, int c)
{
    char *last = 0;
    while (*s)
    {
        if (*s == (char)c)
            last = (char *)s;
        s++;
    }
    if (c == 0)
        return (char *)s;
    return last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;
    for (; *haystack; haystack++)
    {
        if (*haystack == *needle)
        {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n)
            {
                h++;
                n++;
            }
            if (!*n)
                return (char *)haystack;
        }
    }
    return 0;
}

char *strpbrk(const char *s1, const char *s2)
{
    while (*s1)
    {
        for (const char *p = s2; *p; p++)
        {
            if (*s1 == *p)
                return (char *)s1;
        }
        s1++;
    }
    return 0;
}

int memcmp(const void *v1, const void *v2, uint64 n)
{
    const unsigned char *s1 = (const unsigned char *)v1;
    const unsigned char *s2 = (const unsigned char *)v2;
    while (n-- > 0)
    {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++;
        s2++;
    }
    return 0;
}

// =========================================================================
// 4. 新版 GLIBC 2.38+ 引入的 __isoc23_strtol 系列数字解析器轰炸
// =========================================================================
long __isoc23_strtol(const char *nptr, char **endptr, int base)
{
    long res = 0;
    int sign = 1;
    while (*nptr == ' ' || *nptr == '\t')
        nptr++;
    if (*nptr == '-')
    {
        sign = -1;
        nptr++;
    }
    else if (*nptr == '+')
        nptr++;
    if (base == 0)
    {
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X'))
        {
            base = 16;
            nptr += 2;
        }
        else
            base = 10;
    }
    else if (base == 16)
    {
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X'))
            nptr += 2;
    }
    while (*nptr)
    {
        int digit = -1;
        if (*nptr >= '0' && *nptr <= '9')
            digit = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'f')
            digit = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'F')
            digit = *nptr - 'A' + 10;
        if (digit < 0 || digit >= base)
            break;
        res = res * base + digit;
        nptr++;
    }
    if (endptr)
        *endptr = (char *)nptr;
    return res * sign;
}

unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base) { return (unsigned long)__isoc23_strtol(nptr, endptr, base); }
unsigned long long __isoc23_strtoull(const char *nptr, char **endptr, int base) { return (unsigned long long)__isoc23_strtol(nptr, endptr, base); }
long strtol(const char *nptr, char **endptr, int base) { return __isoc23_strtol(nptr, endptr, base); }
unsigned long strtoul(const char *nptr, char **endptr, int base) { return __isoc23_strtoul(nptr, endptr, base); }

double strtod(const char *nptr, char **endptr)
{
    if (endptr)
        *endptr = (char *)nptr;
    return 0.0;
}
long double strtold(const char *nptr, char **endptr)
{
    if (endptr)
        *endptr = (char *)nptr;
    return 0.0;
}
float strtof(const char *nptr, char **endptr)
{
    if (endptr)
        *endptr = (char *)nptr;
    return 0.0f;
}

// =========================================================================
// 5. 符号、动态库、错误跳转桩机制
// =========================================================================
void exit(int status)
{
    syscall(SYS_exit, (uint64)status, 0, 0);
    while (1)
        ;
}

int unlink(const char *pathname) { return syscall(SYS_unlink, (uint64)pathname, 0, 0); }
int remove(const char *pathname) { return unlink(pathname); }

char *getcwd(char *buf, uint64 size)
{
    if (size > 0)
        buf[0] = '/';
    buf[1] = 0;
    return buf;
}
char *getenv(const char *name) { return 0; }
char *realpath(const char *path, char *resolved_path) { return strcpy(resolved_path, path); }

void *dlopen(const char *filename, int flag) { return 0; }
void *dlsym(void *handle, const char *symbol) { return 0; }
int dlclose(void *handle) { return 0; }

int sysconf(int name) { return 4096; }
int mprotect(void *addr, uint64 len, int prot) { return 0; }
void __clear_cache(void *begin, void *end) {}

int _setjmp(void *env) { return 0; }
void longjmp(void *env, int val) { exit(1); }
void __longjmp_chk(void *env, int val) { exit(1); }
void __assert_fail(const char *a, const char *b, unsigned int c, const char *d) { exit(1); }

int *__errno_location(void)
{
    static int local_errno;
    return &local_errno;
}
char *environ[] = {0};
char *strerror(int errnum) { return "error"; }

long time(long *tloc)
{
    if (tloc)
        *tloc = 2026;
    return 2026;
}
void *localtime(const long *timep)
{
    static int dummy[10];
    return dummy;
}
int gettimeofday(void *tv, void *tz) { return 0; }
void qsort(void *base, uint64 nmemb, uint64 size, int (*compar)(const void *, const void *)) {}

// =========================================================================
// 6. 补齐漏掉的信号、信号量与数学库函数 (终极闭环)
// =========================================================================
int sem_init(void *sem, int pshared, unsigned int value) { return 0; }
int sem_wait(void *sem) { return 0; }
int sem_post(void *sem) { return 0; }

int sigemptyset(void *set) { return 0; }
int sigaddset(void *set, int signum) { return 0; }
int sigprocmask(int how, const void *set, void *oldset) { return 0; }
int sigaction(int signum, const void *act, void *oldact) { return 0; }

// 爆破 macro_subst_tok 里面缺失的 long double 指数级乘法函数
long double ldexpl(long double x, int exp)
{
    long double res = x;
    if (exp > 0)
    {
        while (exp--)
            res *= 2.0L;
    }
    else if (exp < 0)
    {
        while (exp++)
            res /= 2.0L;
    }
    return res;
}

// =========================================================================
// 7. 补齐参数解析器需要的字符串转整数函数
// =========================================================================
int atoi(const char *str)
{
    int res = 0;
    int sign = 1;
    // 跳过前导空格
    while (*str == ' ' || *str == '\t' || *str == '\n')
        str++;
    // 处理符号
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
        str++;
    // 转换数字
    while (*str >= '0' && *str <= '9')
    {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}

// =========================================================================
// 8. 真正的裸机程序入口 (充当 crt0，防止 main 返回时跳崖)
// =========================================================================
extern int main(int argc, char **argv);

void _start(int argc, char **argv)
{
    // 1. 调用真正的 main 函数，开始执行 TCC 逻辑
    int ret = main(argc, argv);

    // 2. main 执行完后，绝对不能让它 ret！强制调用咱们的 SYS_exit 系统调用退出
    exit(ret);
}
