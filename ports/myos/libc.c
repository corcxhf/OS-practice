#include <stdarg.h>

typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned int mode_t;
typedef unsigned long uint64;
typedef unsigned int uint;

#define NULL ((void *)0)
#define EOF (-1)

#define SYS_exit 2
#define SYS_sbrk 12
#define SYS_open 15
#define SYS_write 16
#define SYS_read 17
#define SYS_close 18
#define SYS_fstat 19
#define SYS_unlink 24
#define SYS_lseek 25
#define SYS_ftruncate 26
#define SYS_ioctl 27
#define SYS_rename 28
#define SYS_getcwd 29
#define SYS_dup2 30
#define SYS_fcntl 31
#define SYS_fork 1
#define SYS_wait 3
#define SYS_exec 13
#define SYS_dup 23
#define SYS_pipe 22

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x200
#define O_TRUNC 0x400
#define O_EXCL 0x800
#define O_APPEND 0x1000
#define O_ACCMODE 3
#define O_BINARY 0
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define AT_FDCWD -100
#define _SC_PAGESIZE 30
#define F_OK 0
#define R_OK 4

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define TIOCGWINSZ 0x5413
#define TIOCINQ 0x541B
#define ECHO 0000010
#define ICANON 0000002
#define ISIG 0000001
#define SIGWINCH 28
#define ENV_MAX 32

typedef struct FILE
{
    int fd;
    int eof;
    int err;
    int has_unget;
    unsigned char unget;
    int unlink_on_close;
    char unlink_path[128];
} FILE;

struct dirent
{
    unsigned long d_ino;
    char d_name[14];
};

typedef struct DIR
{
    int fd;
    struct dirent ent;
} DIR;

struct myos_dirent
{
    unsigned short inum;
    char name[14];
};

struct winsize
{
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

struct stat
{
    int st_dev;
    unsigned int st_ino;
    short st_mode;
    short st_nlink;
    long st_size;
    long st_atime;
    long st_mtime;
    long st_ctime;
    unsigned int st_uid;
    unsigned int st_gid;
};

struct termios
{
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_cc[32];
};

struct timeval
{
    long tv_sec;
    long tv_usec;
};

struct timezone
{
    int tz_minuteswest;
    int tz_dsttime;
};

struct utimbuf
{
    long actime;
    long modtime;
};

struct tm
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct rusage
{
    struct timeval ru_utime;
    struct timeval ru_stime;
};

struct rlimit
{
    unsigned long rlim_cur;
    unsigned long rlim_max;
};

struct lconv
{
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char int_p_cs_precedes;
    char int_p_sep_by_space;
    char int_n_cs_precedes;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

void free(void *ptr);
void *malloc(size_t nbytes);
void *realloc(void *ptr, size_t nbytes);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long time(long *tloc);
char *getenv(const char *name);
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strchr(const char *s, int c);
char *strcpy(char *dst, const char *src);
int strncmp(const char *a, const char *b, size_t n);
size_t strlen(const char *s);
int tolower(int c);
double frexp(double x, int *exp);
double ldexp(double x, int exp);
static char *copy_cstr(char *dst, size_t cap, const char *src);

char *optarg __attribute__((weak));
int optind __attribute__((weak)) = 1;
int opterr __attribute__((weak)) = 1;
int optopt __attribute__((weak));

static char *env_store[ENV_MAX];
char **environ = env_store;

void __myos_init_environ(char **envp)
{
    int i = 0;

    while (envp && envp[i] && i < ENV_MAX - 1)
    {
        env_store[i] = envp[i];
        i++;
    }
    env_store[i] = NULL;
}

static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm__("a0") = a0;
    register uint64 a1_asm __asm__("a1") = a1;
    register uint64 a2_asm __asm__("a2") = a2;
    register uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall"
                     : "+r"(a0_asm)
                     : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
                     : "memory");
    return a0_asm;
}

static int errno_value;

int *__errno_location(void)
{
    return &errno_value;
}

void exit(int status)
{
    syscall(SYS_exit, (uint64)status, 0, 0);
    for (;;)
        ;
}

void _exit(int status)
{
    exit(status);
}

void *sbrk(long increment)
{
    return (void *)syscall(SYS_sbrk, (uint64)increment, 0, 0);
}

int open(const char *pathname, int flags, ...)
{
    int myos_flags = flags & 0x3;
    int ret;
    if ((flags & 64) || (flags & O_CREAT))
        myos_flags |= O_CREAT;
    if (flags & O_TRUNC)
        myos_flags |= O_TRUNC;
    if (flags & O_EXCL)
        myos_flags |= O_EXCL;
    if (flags & O_APPEND)
        myos_flags |= O_APPEND;
    ret = (int)syscall(SYS_open, (uint64)pathname, (uint64)myos_flags, 0);
    if (ret < 0)
        errno_value = (flags & O_EXCL) ? 17 : 2;
    return ret;
}

int close(int fd)
{
    return (int)syscall(SYS_close, (uint64)fd, 0, 0);
}

ssize_t read(int fd, void *buf, size_t n)
{
    ssize_t ret = (ssize_t)syscall(SYS_read, (uint64)fd, (uint64)buf, n);
    if (ret < 0)
        errno_value = 5;
    return ret;
}

ssize_t write(int fd, const void *buf, size_t n)
{
    ssize_t ret = (ssize_t)syscall(SYS_write, (uint64)fd, (uint64)buf, n);
    if (ret < 0)
        errno_value = 5;
    return ret;
}

long lseek(int fd, long offset, int whence)
{
    return (long)syscall(SYS_lseek, (uint64)fd, (uint64)offset, (uint64)whence);
}

int ftruncate(int fd, long length)
{
    int ret = (int)syscall(SYS_ftruncate, (uint64)fd, (uint64)length, 0);
    if (ret < 0)
        errno_value = 22;
    return ret;
}

int truncate(const char *path, long length)
{
    int fd;
    int ret;

    fd = open(path, O_RDWR);
    if (fd < 0)
        return -1;
    ret = ftruncate(fd, length);
    close(fd);
    return ret;
}

int unlink(const char *pathname)
{
    return (int)syscall(SYS_unlink, (uint64)pathname, 0, 0);
}

int rmdir(const char *pathname)
{
    return unlink(pathname);
}

int remove(const char *pathname)
{
    return unlink(pathname);
}

int rename(const char *oldpath, const char *newpath)
{
    return (int)syscall(SYS_rename, (uint64)oldpath, (uint64)newpath, 0);
}

int fstat(int fd, struct stat *st)
{
    int ret;
    if (!st)
        return -1;
    memset(st, 0, sizeof(*st));
    ret = (int)syscall(SYS_fstat, (uint64)fd, (uint64)st, 0);
    if (ret < 0)
        errno_value = 5;
    else
    {
        st->st_atime = 1;
        st->st_mtime = 1;
        st->st_ctime = 1;
        st->st_uid = 0;
        st->st_gid = 0;
    }
    return ret;
}

int stat(const char *path, struct stat *st)
{
    int fd = open(path, O_RDONLY);
    int ret;

    if (fd < 0)
        return -1;
    ret = fstat(fd, st);
    close(fd);
    return ret;
}

int access(const char *path, int mode)
{
    int fd;
    (void)mode;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    close(fd);
    return 0;
}

int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return (int)syscall(20, (uint64)path, 0, 0);
}

int chdir(const char *path)
{
    return (int)syscall(21, (uint64)path, 0, 0);
}

char *getcwd(char *buf, size_t size)
{
    uint64 ret;

    if (buf == NULL || size == 0)
    {
        errno_value = 22;
        return NULL;
    }
    ret = syscall(SYS_getcwd, (uint64)buf, (uint64)size, 0);
    if (ret == 0)
    {
        errno_value = 22;
        return NULL;
    }
    return buf;
}

int dup(int oldfd)
{
    return (int)syscall(SYS_dup, (uint64)oldfd, 0, 0);
}

int dup2(int oldfd, int newfd)
{
    return (int)syscall(SYS_dup2, (uint64)oldfd, (uint64)newfd, 0);
}

int getpid(void)
{
    return (int)syscall(11, 0, 0, 0);
}

unsigned int getuid(void)
{
    return 0;
}

unsigned int getgid(void)
{
    return 0;
}

int fork(void)
{
    return (int)syscall(SYS_fork, 0, 0, 0);
}

int pipe(int pipefd[2])
{
    return (int)syscall(SYS_pipe, (uint64)pipefd, 0, 0);
}

int waitpid(int pid, int *status, int options)
{
    (void)pid;
    (void)options;
    return (int)syscall(SYS_wait, (uint64)status, 0, 0);
}

int wait(int *status)
{
    return waitpid(-1, status, 0);
}

int kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return -1;
}

unsigned int sleep(unsigned int seconds)
{
    return seconds;
}

int execvp(const char *file, char *const argv[])
{
    char path[128];
    const char *env_path;
    int has_slash = 0;

    if (!file || !*file)
    {
        errno_value = 2;
        return -1;
    }

    for (const char *p = file; *p; p++)
        if (*p == '/')
            has_slash = 1;
    if (has_slash)
        return (int)syscall(SYS_exec, (uint64)file, (uint64)argv, (uint64)environ);

    env_path = getenv("PATH");
    if (!env_path || !*env_path)
        env_path = "/bin:/";

    while (*env_path)
    {
        int dir_len = 0;
        int n = 0;

        while (env_path[dir_len] && env_path[dir_len] != ':')
            dir_len++;

        if (dir_len == 0)
        {
            path[n++] = '.';
        }
        else
        {
            for (int i = 0; i < dir_len && n < (int)sizeof(path) - 1; i++)
                path[n++] = env_path[i];
        }
        if (n > 0 && path[n - 1] != '/' && n < (int)sizeof(path) - 1)
            path[n++] = '/';
        for (int i = 0; file[i] && n < (int)sizeof(path) - 1; i++)
            path[n++] = file[i];
        path[n] = '\0';

        if (n < (int)sizeof(path) - 1)
            syscall(SYS_exec, (uint64)path, (uint64)argv, (uint64)environ);

        env_path += dir_len;
        if (*env_path == ':')
            env_path++;
    }

    errno_value = 2;
    return -1;
}

int execv(const char *path, char *const argv[])
{
    return (int)syscall(SYS_exec, (uint64)path, (uint64)argv, (uint64)environ);
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    return (int)syscall(SYS_exec, (uint64)path, (uint64)argv, (uint64)(envp ? envp : environ));
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    (void)dirfd;
    return open(pathname, flags);
}

int faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    (void)dirfd;
    (void)flags;
    return access(pathname, mode);
}

int fstatat(int dirfd, const char *pathname, struct stat *st, int flags)
{
    (void)dirfd;
    (void)flags;
    return stat(pathname, st);
}

int fcntl(int fd, int cmd, ...)
{
    va_list ap;
    uint64 arg = 0;
    int ret;

    if (cmd == F_DUPFD || cmd == F_SETFD || cmd == F_SETFL)
    {
        va_start(ap, cmd);
        arg = (uint64)va_arg(ap, int);
        va_end(ap);
    }

    ret = (int)syscall(SYS_fcntl, (uint64)fd, (uint64)cmd, arg);
    if (ret < 0)
        errno_value = 22;
    return ret;
}

int chmod(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return 0;
}

int fchmod(int fd, mode_t mode)
{
    (void)fd;
    (void)mode;
    return 0;
}

int utime(const char *filename, const struct utimbuf *times)
{
    (void)times;
    return access(filename, F_OK);
}

mode_t umask(mode_t mode)
{
    (void)mode;
    return 0;
}

int readlink(const char *path, char *buf, size_t bufsiz)
{
    (void)path;
    (void)buf;
    (void)bufsiz;
    errno_value = 22;
    return -1;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s && s[n])
        n++;
    return n;
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t n = 0;
    while (n < maxlen && s && s[n])
        n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n > 0 && *a && *a == *b)
    {
        a++;
        b++;
        n--;
    }
    return n == 0 ? 0 : *(const unsigned char *)a - *(const unsigned char *)b;
}

int strcoll(const char *a, const char *b)
{
    return strcmp(a, b);
}

size_t strxfrm(char *dst, const char *src, size_t n)
{
    size_t len = strlen(src);
    size_t copy = len;

    if (n == 0)
        return len;
    if (copy >= n)
        copy = n - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return len;
}

int strcasecmp(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b))
    {
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    while (n > 0 && *a && tolower((unsigned char)*a) == tolower((unsigned char)*b))
    {
        a++;
        b++;
        n--;
    }
    return n == 0 ? 0 : tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++))
        ;
    return ret;
}

static char *copy_cstr(char *dst, size_t cap, const char *src)
{
    size_t i = 0;

    if (cap == 0)
        return dst;
    while (src && src[i] && i + 1 < cap)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *ret = dst;
    while (n > 0 && *src)
    {
        *dst++ = *src++;
        n--;
    }
    while (n-- > 0)
        *dst++ = '\0';
    return ret;
}

char *stpcpy(char *dst, const char *src)
{
    while ((*dst = *src) != '\0')
    {
        dst++;
        src++;
    }
    return dst;
}

char *stpncpy(char *dst, const char *src, size_t n)
{
    char *p = dst;
    while (n > 0 && *src)
    {
        *p++ = *src++;
        n--;
    }
    while (n > 0)
    {
        *p++ = '\0';
        n--;
    }
    return p;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p)
        return NULL;
    memcpy(p, s, n);
    return p;
}

char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *p = malloc(len + 1);
    if (!p)
        return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

char *strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

char *strncat(char *dst, const char *src, size_t n)
{
    char *p = dst + strlen(dst);
    while (n > 0 && *src)
    {
        *p++ = *src++;
        n--;
    }
    *p = '\0';
    return dst;
}

static int char_in_set(int c, const char *set)
{
    while (*set)
    {
        if ((unsigned char)*set == (unsigned char)c)
            return 1;
        set++;
    }
    return 0;
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;
    while (s[n] && char_in_set(s[n], accept))
        n++;
    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;
    while (s[n] && !char_in_set(s[n], reject))
        n++;
    return n;
}

char *strpbrk(const char *s, const char *accept)
{
    while (*s)
    {
        if (char_in_set(*s, accept))
            return (char *)s;
        s++;
    }
    return NULL;
}

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    return c == 0 ? (char *)s : NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    do
    {
        if (*s == (char)c)
            last = s;
    } while (*s++);
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (*needle == 0)
        return (char *)haystack;
    for (; *haystack; haystack++)
    {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n)
        {
            h++;
            n++;
        }
        if (*n == 0)
            return (char *)haystack;
    }
    return NULL;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *p = dst;
    while (n--)
        *p++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *mempcpy(void *dst, const void *src, size_t n)
{
    memcpy(dst, src, n);
    return (char *)dst + n;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s)
    {
        while (n--)
            *d++ = *s++;
    }
    else
    {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    while (n--)
    {
        if (*p == (unsigned char)c)
            return (void *)p;
        p++;
    }
    return NULL;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a;
    const unsigned char *q = b;
    while (n--)
    {
        if (*p != *q)
            return *p - *q;
        p++;
        q++;
    }
    return 0;
}

int bcmp(const void *a, const void *b, size_t n)
{
    return memcmp(a, b, n);
}

void bcopy(const void *src, void *dst, size_t n)
{
    memmove(dst, src, n);
}

void bzero(void *s, size_t n)
{
    memset(s, 0, n);
}

int ffs(int i)
{
    if (i == 0)
        return 0;
    int bit = 1;
    unsigned int x = (unsigned int)i;
    while ((x & 1U) == 0)
    {
        x >>= 1;
        bit++;
    }
    return bit;
}

char *index(const char *s, int c)
{
    return strchr(s, c);
}

char *rindex(const char *s, int c)
{
    return strrchr(s, c);
}

typedef long Align;

union header
{
    struct
    {
        union header *ptr;
        size_t size;
    } s;
    Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

static Header *morecore(size_t nunits)
{
    char *cp;
    Header *up;

    if (nunits < 1024)
        nunits = 1024;

    cp = sbrk((long)(nunits * sizeof(Header)));
    if (cp == (char *)-1)
        return NULL;

    up = (Header *)cp;
    up->s.size = nunits;
    free((void *)(up + 1));
    return freep;
}

void free(void *ptr)
{
    Header *bp;
    Header *p;

    if (ptr == NULL)
        return;

    if (freep == NULL)
    {
        base.s.ptr = freep = &base;
        base.s.size = 0;
    }

    bp = (Header *)ptr - 1;
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    {
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    }

    if (bp + bp->s.size == p->s.ptr)
    {
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    }
    else
    {
        bp->s.ptr = p->s.ptr;
    }

    if (p + p->s.size == bp)
    {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    }
    else
    {
        p->s.ptr = bp;
    }
    freep = p;
}

void *malloc(size_t nbytes)
{
    Header *p;
    Header *prevp;
    size_t nunits;

    if (nbytes == 0)
        nbytes = 1;
    nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

    if ((prevp = freep) == NULL)
    {
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }

    for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr)
    {
        if (p->s.size >= nunits)
        {
            if (p->s.size == nunits)
            {
                prevp->s.ptr = p->s.ptr;
            }
            else
            {
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void *)(p + 1);
        }

        if (p == freep)
        {
            p = morecore(nunits);
            if (p == NULL)
                return NULL;
        }
    }
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t nbytes)
{
    Header *bp;
    size_t old_bytes;
    void *new_ptr;

    if (ptr == NULL)
        return malloc(nbytes);
    if (nbytes == 0)
    {
        free(ptr);
        return NULL;
    }

    bp = (Header *)ptr - 1;
    old_bytes = (bp->s.size - 1) * sizeof(Header);
    if (old_bytes >= nbytes)
        return ptr;

    new_ptr = malloc(nbytes);
    if (new_ptr == NULL)
        return NULL;
    memcpy(new_ptr, ptr, old_bytes);
    free(ptr);
    return new_ptr;
}

static FILE std_files[3] = {
    {0, 0, 0, 0, 0, 0, {0}},
    {1, 0, 0, 0, 0, 0, {0}},
    {2, 0, 0, 0, 0, 0, {0}},
};

FILE *stdin = &std_files[0];
FILE *stdout = &std_files[1];
FILE *stderr = &std_files[2];
static unsigned int tty_lflag = ECHO | ICANON | ISIG;

static int mode_to_flags(const char *mode)
{
    int plus = strchr(mode, '+') != NULL;
    if (mode[0] == 'r')
        return plus ? O_RDWR : O_RDONLY;
    if (mode[0] == 'w')
        return (plus ? O_RDWR : O_WRONLY) | O_CREAT;
    if (mode[0] == 'a')
        return (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
    return O_RDONLY;
}

FILE *fopen(const char *filename, const char *mode)
{
    int fd;
    FILE *f;
    int flags = mode_to_flags(mode);

    if (mode[0] == 'w')
        unlink(filename);

    fd = open(filename, flags);
    if (fd < 0)
        return NULL;
    if (mode[0] == 'a')
        lseek(fd, 0, SEEK_END);

    f = malloc(sizeof(FILE));
    if (f == NULL)
    {
        close(fd);
        return NULL;
    }
    f->fd = fd;
    f->eof = 0;
    f->err = 0;
    f->has_unget = 0;
    f->unget = 0;
    f->unlink_on_close = 0;
    f->unlink_path[0] = 0;
    return f;
}

FILE *fdopen(int fd, const char *mode)
{
    FILE *f;
    (void)mode;
    if (fd < 0)
        return NULL;
    f = malloc(sizeof(FILE));
    if (f == NULL)
        return NULL;
    f->fd = fd;
    f->eof = 0;
    f->err = 0;
    f->has_unget = 0;
    f->unget = 0;
    f->unlink_on_close = 0;
    f->unlink_path[0] = 0;
    return f;
}

static int fill_temp_template(char *template, int suffixlen, int create_dir)
{
    static unsigned long temp_counter;
    static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int len = (int)strlen(template);
    int x_end = len - suffixlen;
    int x_start = x_end - 6;

    if (suffixlen < 0 || x_start < 0)
    {
        errno_value = 22;
        return -1;
    }
    for (int i = x_start; i < x_end; i++)
    {
        if (template[i] != 'X')
        {
            errno_value = 22;
            return -1;
        }
    }

    for (unsigned long attempt = 0; attempt < 4096; attempt++)
    {
        unsigned long value = temp_counter++ + (unsigned long)getpid() * 131 + attempt;
        int fd;

        for (int i = x_end - 1; i >= x_start; i--)
        {
            template[i] = chars[value % (sizeof(chars) - 1)];
            value /= sizeof(chars) - 1;
        }

        if (create_dir)
        {
            if (mkdir(template, 0700) == 0)
                return 0;
        }
        else
        {
            fd = open(template, O_RDWR | O_CREAT | O_EXCL);
            if (fd >= 0)
                return fd;
        }
    }

    errno_value = 17;
    return -1;
}

int mkstemps(char *template, int suffixlen)
{
    return fill_temp_template(template, suffixlen, 0);
}

int mkstemp(char *template)
{
    return mkstemps(template, 0);
}

char *mkdtemp(char *template)
{
    return fill_temp_template(template, 0, 1) == 0 ? template : NULL;
}

FILE *tmpfile(void)
{
    char path[] = "/tmp/tmpXXXXXX";
    int fd = mkstemp(path);
    FILE *f;

    if (fd < 0)
        return NULL;
    f = fdopen(fd, "w+");
    if (!f)
    {
        close(fd);
        unlink(path);
        return NULL;
    }
    f->unlink_on_close = 1;
    copy_cstr(f->unlink_path, sizeof(f->unlink_path), path);
    return f;
}

int fclose(FILE *stream)
{
    int ret = 0;
    if (stream == NULL)
        return EOF;
    if (stream != stdin && stream != stdout && stream != stderr)
    {
        ret = close(stream->fd);
        if (stream->unlink_on_close)
            unlink(stream->unlink_path);
        free(stream);
    }
    return ret;
}

int fileno(FILE *stream)
{
    return stream ? stream->fd : -1;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    ssize_t ret;
    size_t bytes = size * nmemb;
    unsigned char *out = ptr;
    size_t done = 0;
    if (bytes == 0)
        return 0;
    if (stream->has_unget)
    {
        out[done++] = stream->unget;
        stream->has_unget = 0;
        if (done == bytes)
            return done / size;
    }
    ret = read(stream->fd, out + done, bytes - done);
    if (ret < 0)
    {
        stream->err = 1;
        return 0;
    }
    done += (size_t)ret;
    if (done < bytes)
        stream->eof = 1;
    return done / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    ssize_t ret;
    size_t bytes = size * nmemb;
    if (bytes == 0)
        return 0;
    ret = write(stream->fd, ptr, bytes);
    if (ret < 0)
    {
        stream->err = 1;
        return 0;
    }
    return (size_t)ret / size;
}

int fgetc(FILE *stream)
{
    unsigned char ch;
    ssize_t ret = read(stream->fd, &ch, 1);
    if (stream->has_unget)
    {
        stream->has_unget = 0;
        return stream->unget;
    }
    if (ret == 1)
    {
        if (stream == stdin && (tty_lflag & ECHO))
            write(stdout->fd, &ch, 1);
        return ch;
    }
    if (ret == 0)
        stream->eof = 1;
    else
        stream->err = 1;
    return EOF;
}

int getc(FILE *stream)
{
    return fgetc(stream);
}

int getchar(void)
{
    return fgetc(stdin);
}

char *fgets(char *s, int size, FILE *stream)
{
    int i = 0;

    if (!s || size <= 0)
        return NULL;
    while (i + 1 < size)
    {
        int c = fgetc(stream);
        if (c == EOF)
            break;
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    if (i == 0)
        return NULL;
    s[i] = '\0';
    return s;
}

int ungetc(int c, FILE *stream)
{
    if (!stream || c == EOF || stream->has_unget)
        return EOF;
    stream->has_unget = 1;
    stream->unget = (unsigned char)c;
    stream->eof = 0;
    return c;
}

int fputc(int c, FILE *stream)
{
    unsigned char ch = (unsigned char)c;
    return write(stream->fd, &ch, 1) == 1 ? c : EOF;
}

int putc(int c, FILE *stream)
{
    return fputc(c, stream);
}

int putchar(int c)
{
    return fputc(c, stdout);
}

int fputs(const char *s, FILE *stream)
{
    size_t n = strlen(s);
    return write(stream->fd, s, n) == (ssize_t)n ? 0 : EOF;
}

int puts(const char *s)
{
    if (fputs(s, stdout) < 0)
        return EOF;
    return fputc('\n', stdout);
}

int fflush(FILE *stream)
{
    (void)stream;
    return 0;
}

int fseek(FILE *stream, long offset, int whence)
{
    long ret = lseek(stream->fd, offset, whence);
    if (ret < 0)
    {
        stream->err = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream)
{
    return lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream)
{
    fseek(stream, 0, SEEK_SET);
}

int feof(FILE *stream)
{
    return stream->eof;
}

int ferror(FILE *stream)
{
    return stream->err;
}

void clearerr(FILE *stream)
{
    stream->eof = 0;
    stream->err = 0;
}

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
    size_t len = 0;
    int c;

    if (lineptr == NULL || n == NULL || stream == NULL)
        return -1;

    if (*lineptr == NULL || *n == 0)
    {
        *n = 128;
        *lineptr = malloc(*n);
        if (*lineptr == NULL)
            return -1;
    }

    while ((c = fgetc(stream)) != EOF)
    {
        if (len + 1 >= *n)
        {
            size_t next_size = *n * 2;
            char *next = realloc(*lineptr, next_size);
            if (next == NULL)
                return -1;
            *lineptr = next;
            *n = next_size;
        }
        (*lineptr)[len++] = (char)c;
        if (c == delim)
            break;
    }

    if (len == 0 && c == EOF)
        return -1;

    (*lineptr)[len] = '\0';
    return (ssize_t)len;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    return getdelim(lineptr, n, '\n', stream);
}

static void out_char(char **buf, size_t *left, int *count, int c)
{
    if (*left > 1)
    {
        **buf = (char)c;
        (*buf)++;
        (*left)--;
    }
    (*count)++;
}

static void out_str(char **buf, size_t *left, int *count, const char *s)
{
    if (s == NULL)
        s = "(null)";
    while (*s)
        out_char(buf, left, count, *s++);
}

static void out_repeat(char **buf, size_t *left, int *count, int c, int n)
{
    while (n-- > 0)
        out_char(buf, left, count, c);
}

static void out_uint(char **buf, size_t *left, int *count, unsigned long x, int base,
                     int neg, int left_align, int pad_zero, int width)
{
    char tmp[32];
    int n = 0;
    int len;
    int pad;

    if (x == 0)
    {
        tmp[n++] = '0';
    }
    else
    {
        while (x)
        {
            int digit = x % (unsigned long)base;
            tmp[n++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            x /= (unsigned long)base;
        }
    }

    len = n + (neg ? 1 : 0);
    pad = width > len ? width - len : 0;

    if (!left_align && !pad_zero)
        out_repeat(buf, left, count, ' ', pad);
    if (neg)
        out_char(buf, left, count, '-');
    if (!left_align && pad_zero)
        out_repeat(buf, left, count, '0', pad);
    while (n--)
        out_char(buf, left, count, tmp[n]);
    if (left_align)
        out_repeat(buf, left, count, ' ', pad);
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    char *out = str;
    size_t left = size;
    int count = 0;

    if (size == 0)
    {
        static char sink;
        out = &sink;
        left = 0;
    }

    while (*fmt)
    {
        int long_arg = 0;
        int left_align = 0;
        int pad_zero = 0;
        int width = 0;
        if (*fmt != '%')
        {
            out_char(&out, &left, &count, *fmt++);
            continue;
        }

        fmt++;
        while (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#' || *fmt == '0')
        {
            if (*fmt == '-')
                left_align = 1;
            else if (*fmt == '0')
                pad_zero = 1;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (*fmt == '.')
        {
            fmt++;
            while (*fmt >= '0' && *fmt <= '9')
                fmt++;
        }
        while (*fmt == 'l' || *fmt == 'z')
        {
            long_arg = 1;
            fmt++;
        }

        if (*fmt == 's')
        {
            out_str(&out, &left, &count, va_arg(ap, const char *));
        }
        else if (*fmt == 'c')
        {
            out_char(&out, &left, &count, va_arg(ap, int));
        }
        else if (*fmt == 'd' || *fmt == 'i')
        {
            long v = long_arg ? va_arg(ap, long) : va_arg(ap, int);
            unsigned long x = v < 0 ? (unsigned long)-v : (unsigned long)v;
            out_uint(&out, &left, &count, x, 10, v < 0, left_align, pad_zero, width);
        }
        else if (*fmt == 'u')
        {
            unsigned long x = long_arg ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            out_uint(&out, &left, &count, x, 10, 0, left_align, pad_zero, width);
        }
        else if (*fmt == 'x' || *fmt == 'X')
        {
            unsigned long x = long_arg ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            out_uint(&out, &left, &count, x, 16, 0, left_align, pad_zero, width);
        }
        else if (*fmt == 'o')
        {
            unsigned long x = long_arg ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            out_uint(&out, &left, &count, x, 8, 0, left_align, pad_zero, width);
        }
        else if (*fmt == 'p')
        {
            out_str(&out, &left, &count, "0x");
            out_uint(&out, &left, &count, (unsigned long)va_arg(ap, void *), 16, 0, 0, 0, 0);
        }
        else if (*fmt == '%')
        {
            out_char(&out, &left, &count, '%');
        }
        else
        {
            out_char(&out, &left, &count, '%');
            if (*fmt)
                out_char(&out, &left, &count, *fmt);
        }

        if (*fmt)
            fmt++;
    }

    if (size > 0)
    {
        if (left > 0)
            *out = '\0';
        else
            str[size - 1] = '\0';
    }
    return count;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsnprintf(str, (size_t)-1, fmt, ap);
    va_end(ap);
    return ret;
}

int vsprintf(char *str, const char *fmt, va_list ap)
{
    return vsnprintf(str, (size_t)-1, fmt, ap);
}

int __attribute__((weak)) vasprintf(char **strp, const char *fmt, va_list ap)
{
    va_list copy;
    int len;
    char *buf;

    va_copy(copy, ap);
    len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (len < 0)
        return -1;
    buf = malloc((size_t)len + 1);
    if (!buf)
    {
        errno_value = 12;
        return -1;
    }
    vsnprintf(buf, (size_t)len + 1, fmt, ap);
    *strp = buf;
    return len;
}

int __attribute__((weak)) asprintf(char **strp, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = vasprintf(strp, fmt, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    char buf[1024];
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    write(stream->fd, buf, strlen(buf));
    return ret;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vfprintf(stream, fmt, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char *fmt, va_list ap)
{
    return vfprintf(stdout, fmt, ap);
}

int printf(const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return ret;
}

static int scan_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int vscan_string(const char *str, const char *fmt, va_list ap)
{
    int count = 0;

    while (*fmt)
    {
        if (scan_space((unsigned char)*fmt))
        {
            while (scan_space((unsigned char)*fmt))
                fmt++;
            while (scan_space((unsigned char)*str))
                str++;
            continue;
        }
        if (*fmt != '%')
        {
            if (*str != *fmt)
                break;
            str++;
            fmt++;
            continue;
        }

        fmt++;
        int assign = 1;
        int length = 0;
        if (*fmt == '*')
        {
            assign = 0;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9')
            fmt++;
        if (*fmt == 'h')
        {
            fmt++;
            length = -1;
            if (*fmt == 'h')
            {
                fmt++;
                length = -2;
            }
        }
        else if (*fmt == 'l')
        {
            fmt++;
            length = 1;
            if (*fmt == 'l')
            {
                fmt++;
                length = 2;
            }
        }
        else if (*fmt == 'j' || *fmt == 'z' || *fmt == 't')
        {
            fmt++;
            length = 1;
        }
        if (*fmt == 'd' || *fmt == 'i' || *fmt == 'u' || *fmt == 'o' || *fmt == 'x' || *fmt == 'X')
        {
            void *out = assign ? va_arg(ap, void *) : NULL;
            char *end;
            int base = 10;
            unsigned long v;
            int is_signed = *fmt == 'd' || *fmt == 'i';
            while (scan_space((unsigned char)*str))
                str++;
            if (*fmt == 'o')
                base = 8;
            else if (*fmt == 'x' || *fmt == 'X')
                base = 16;
            else if (*fmt == 'i')
                base = 0;
            v = is_signed ? (unsigned long)strtol(str, &end, base) : strtoul(str, &end, base);
            if (end == str)
                break;
            if (assign)
            {
                if (length >= 1)
                    *(unsigned long *)out = v;
                else if (length == -1)
                    *(unsigned short *)out = (unsigned short)v;
                else if (length == -2)
                    *(unsigned char *)out = (unsigned char)v;
                else
                    *(unsigned int *)out = (unsigned int)v;
                count++;
            }
            str = end;
        }
        else if (*fmt == 's')
        {
            char *out = va_arg(ap, char *);
            while (scan_space((unsigned char)*str))
                str++;
            if (!*str)
                break;
            while (*str && !scan_space((unsigned char)*str))
                *out++ = *str++;
            *out = '\0';
            count++;
        }
        else if (*fmt == 'c')
        {
            char *out = va_arg(ap, char *);
            if (!*str)
                break;
            *out = *str++;
            count++;
        }
        else if (*fmt == '%')
        {
            if (*str != '%')
                break;
            str++;
        }
        else
        {
            break;
        }
        fmt++;
    }
    return count;
}

int sscanf(const char *str, const char *fmt, ...)
{
    va_list ap;
    int count;

    va_start(ap, fmt);
    count = vscan_string(str, fmt, ap);
    va_end(ap);
    return count;
}

int scanf(const char *fmt, ...)
{
    char buf[256];
    int len = 0;
    int count;
    va_list ap;

    while (len + 1 < (int)sizeof(buf))
    {
        int c = getchar();
        if (c == EOF)
            break;
        buf[len++] = (char)c;
        if (c == '\n')
            break;
    }
    buf[len] = '\0';

    va_start(ap, fmt);
    count = vscan_string(buf, fmt, ap);
    va_end(ap);
    return count;
}

void perror(const char *s)
{
    if (s && *s)
    {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs("error\n", stderr);
}

long strtol(const char *nptr, char **endptr, int base)
{
    long result = 0;
    int sign = 1;

    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n')
        nptr++;
    if (*nptr == '-')
    {
        sign = -1;
        nptr++;
    }
    else if (*nptr == '+')
    {
        nptr++;
    }

    if (base == 0)
    {
        if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X'))
        {
            base = 16;
            nptr += 2;
        }
        else
        {
            base = 10;
        }
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
        result = result * base + digit;
        nptr++;
    }

    if (endptr)
        *endptr = (char *)nptr;
    return result * sign;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long)strtol(nptr, endptr, base);
}

long long strtoll(const char *nptr, char **endptr, int base)
{
    return (long long)strtol(nptr, endptr, base);
}

unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
    return (unsigned long long)strtoul(nptr, endptr, base);
}

double strtod(const char *nptr, char **endptr)
{
    char *parse_end;
    long whole = strtol(nptr, &parse_end, 10);
    double value = (double)whole;
    const char *p = parse_end;
    double place = 0.1;
    int neg = nptr && *nptr == '-';

    if (*p == '.')
    {
        p++;
        while (*p >= '0' && *p <= '9')
        {
            if (neg)
                value -= (*p - '0') * place;
            else
                value += (*p - '0') * place;
            place *= 0.1;
            p++;
        }
    }
    if (endptr)
        *endptr = (char *)p;
    return value;
}

float strtof(const char *nptr, char **endptr)
{
    return (float)strtod(nptr, endptr);
}

long double strtold(const char *nptr, char **endptr)
{
    (void)strtod(nptr, endptr);
    return 0.0L;
}

double atof(const char *nptr)
{
    return strtod(nptr, NULL);
}

long strtoimax(const char *nptr, char **endptr, int base)
{
    return strtol(nptr, endptr, base);
}

unsigned long strtoumax(const char *nptr, char **endptr, int base)
{
    return strtoul(nptr, endptr, base);
}

struct imaxdiv_result
{
    long quot;
    long rem;
};

struct imaxdiv_result imaxdiv(long numer, long denom)
{
    struct imaxdiv_result result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

int atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}

long atol(const char *s)
{
    return strtol(s, NULL, 10);
}

long long atoll(const char *s)
{
    return strtoll(s, NULL, 10);
}

int abs(int x)
{
    return x < 0 ? -x : x;
}

long labs(long x)
{
    return x < 0 ? -x : x;
}

char *getenv(const char *name)
{
    size_t name_len;

    if (!name || name[0] == '\0' || strchr(name, '='))
        return NULL;
    name_len = strlen(name);
    for (int i = 0; environ[i]; i++)
    {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=')
            return environ[i] + name_len + 1;
    }
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite)
{
    size_t name_len;
    size_t value_len;
    char *entry;

    if (!name || name[0] == '\0' || strchr(name, '='))
    {
        errno_value = 22;
        return -1;
    }
    if (!value)
        value = "";

    name_len = strlen(name);
    for (int i = 0; environ[i]; i++)
    {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=')
        {
            if (!overwrite)
                return 0;
            value_len = strlen(value);
            entry = malloc(name_len + value_len + 2);
            if (!entry)
            {
                errno_value = 12;
                return -1;
            }
            strcpy(entry, name);
            entry[name_len] = '=';
            strcpy(entry + name_len + 1, value);
            environ[i] = entry;
            return 0;
        }
    }

    for (int i = 0; i < ENV_MAX - 1; i++)
    {
        if (!environ[i])
        {
            value_len = strlen(value);
            entry = malloc(name_len + value_len + 2);
            if (!entry)
            {
                errno_value = 12;
                return -1;
            }
            strcpy(entry, name);
            entry[name_len] = '=';
            strcpy(entry + name_len + 1, value);
            environ[i] = entry;
            environ[i + 1] = NULL;
            return 0;
        }
    }

    errno_value = 12;
    return -1;
}

int unsetenv(const char *name)
{
    size_t name_len;

    if (!name || name[0] == '\0' || strchr(name, '='))
    {
        errno_value = 22;
        return -1;
    }

    name_len = strlen(name);
    for (int i = 0; environ[i];)
    {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=')
        {
            for (int j = i; environ[j]; j++)
                environ[j] = environ[j + 1];
            continue;
        }
        i++;
    }
    return 0;
}

int putenv(char *string)
{
    char *eq;
    size_t name_len;

    if (!string || !(eq = strchr(string, '=')) || eq == string)
    {
        errno_value = 22;
        return -1;
    }
    name_len = eq - string;

    for (int i = 0; environ[i]; i++)
    {
        if (strncmp(environ[i], string, name_len) == 0 && environ[i][name_len] == '=')
        {
            environ[i] = string;
            return 0;
        }
    }

    for (int i = 0; i < ENV_MAX - 1; i++)
    {
        if (!environ[i])
        {
            environ[i] = string;
            environ[i + 1] = NULL;
            return 0;
        }
    }

    errno_value = 12;
    return -1;
}

char *realpath(const char *path, char *resolved_path)
{
    char cwd_buf[128];
    char *out = resolved_path;
    size_t pos = 0;

    if (!path)
        return NULL;
    if (!out)
    {
        out = malloc(128);
        if (!out)
            return NULL;
    }

    if (path[0] == '/')
    {
        copy_cstr(out, 128, path);
        return out;
    }

    if (!getcwd(cwd_buf, sizeof(cwd_buf)))
    {
        if (!resolved_path)
            free(out);
        return NULL;
    }
    copy_cstr(out, 128, cwd_buf);
    pos = strlen(out);
    if (pos + 1 < 128 && !(pos == 1 && out[0] == '/'))
        out[pos++] = '/';
    copy_cstr(out + pos, 128 - pos, path);
    return out;
}

static void swap_bytes(char *a, char *b, size_t size)
{
    while (size--)
    {
        char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    char *items = base;

    if (!items || size == 0 || !compar)
        return;
    for (size_t i = 1; i < nmemb; i++)
    {
        size_t j = i;
        while (j > 0 && compar(items + j * size, items + (j - 1) * size) < 0)
        {
            swap_bytes(items + j * size, items + (j - 1) * size, size);
            j--;
        }
    }
}

long sysconf(int name)
{
    if (name == _SC_PAGESIZE)
        return 4096;
    errno_value = 22;
    return -1;
}

char *setlocale(int category, const char *locale)
{
    static char c_locale[] = "C";
    (void)category;

    if (!locale || locale[0] == '\0' || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0)
        return c_locale;
    return NULL;
}

struct lconv *localeconv(void)
{
    static char dot[] = ".";
    static char empty[] = "";
    static struct lconv c_lconv = {
        dot,
        empty,
        empty,
        empty,
        empty,
        empty,
        empty,
        empty,
        empty,
        empty,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
        127,
    };

    return &c_lconv;
}

long pathconf(const char *path, int name)
{
    (void)path;
    if (name == 4)
        return 128;
    errno_value = 22;
    return -1;
}

int getpagesize(void)
{
    return 4096;
}

int gettimeofday(struct timeval *tv, void *tz)
{
    if (tv)
    {
        tv->tv_sec = time(NULL);
        tv->tv_usec = 0;
    }
    if (tz)
    {
        struct timezone *zone = tz;
        zone->tz_minuteswest = 0;
        zone->tz_dsttime = 0;
    }
    return 0;
}

int getrusage(int who, struct rusage *usage)
{
    (void)who;
    if (usage)
        memset(usage, 0, sizeof(*usage));
    return 0;
}

int getrlimit(int resource, struct rlimit *rlim)
{
    (void)resource;
    if (rlim)
    {
        rlim->rlim_cur = (unsigned long)-1;
        rlim->rlim_max = (unsigned long)-1;
    }
    return 0;
}

int setrlimit(int resource, const struct rlimit *rlim)
{
    (void)resource;
    (void)rlim;
    return 0;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset)
{
    void *p;
    (void)addr;
    (void)prot;
    (void)flags;
    (void)fd;
    (void)offset;
    p = malloc(length ? length : 1);
    return p ? p : (void *)-1;
}

int munmap(void *addr, size_t length)
{
    (void)length;
    if (addr && addr != (void *)-1)
        free(addr);
    return 0;
}

static unsigned long rand_state = 1;

void srand(unsigned int seed)
{
    rand_state = seed ? seed : 1;
}

int rand(void)
{
    rand_state = rand_state * 1103515245UL + 12345UL;
    return (int)((rand_state >> 16) & 0x7fff);
}

int isnan(double x)
{
    return __builtin_isnan(x);
}

int isinf(double x)
{
    return __builtin_isinf(x);
}

int finite(double x)
{
    return __builtin_isfinite(x);
}

double fabs(double x)
{
    return x < 0.0 ? -x : x;
}

double frexp(double x, int *exp)
{
    int e = 0;
    double ax;

    if (x == 0.0 || isnan(x) || isinf(x))
    {
        if (exp)
            *exp = 0;
        return x;
    }

    ax = fabs(x);
    while (ax >= 1.0)
    {
        x *= 0.5;
        ax *= 0.5;
        e++;
    }
    while (ax < 0.5)
    {
        x *= 2.0;
        ax *= 2.0;
        e--;
    }
    if (exp)
        *exp = e;
    return x;
}

double ldexp(double x, int exp)
{
    if (exp > 0)
    {
        while (exp--)
            x *= 2.0;
    }
    else
    {
        while (exp++)
            x *= 0.5;
    }
    return x;
}

void abort(void)
{
    exit(134);
}

int __attribute__((weak)) getopt(int argc, char *const argv[], const char *optstring)
{
    static int optpos = 1;
    const char *match;
    char opt;

    if (optind <= 0)
        optind = 1;
    if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || argv[optind][1] == '\0')
        return -1;
    if (strcmp(argv[optind], "--") == 0)
    {
        optind++;
        return -1;
    }

    opt = argv[optind][optpos++];
    optopt = opt;
    match = strchr(optstring, opt);
    if (!match || opt == ':')
    {
        if (argv[optind][optpos] == '\0')
        {
            optind++;
            optpos = 1;
        }
        return '?';
    }

    if (match[1] == ':')
    {
        if (argv[optind][optpos] != '\0')
        {
            optarg = &argv[optind][optpos];
            optind++;
            optpos = 1;
        }
        else if (optind + 1 < argc)
        {
            optarg = argv[++optind];
            optind++;
            optpos = 1;
        }
        else
        {
            return optstring[0] == ':' ? ':' : '?';
        }
    }
    else
    {
        optarg = NULL;
        if (argv[optind][optpos] == '\0')
        {
            optind++;
            optpos = 1;
        }
    }
    return opt;
}

long time(long *tloc)
{
    if (tloc)
        *tloc = 2026;
    return 2026;
}

long clock(void)
{
    return 0;
}

static struct tm *epoch_tm(void)
{
    static struct tm tm;

    tm.tm_sec = 0;
    tm.tm_min = 0;
    tm.tm_hour = 0;
    tm.tm_mday = 1;
    tm.tm_mon = 0;
    tm.tm_year = 70;
    tm.tm_wday = 4;
    tm.tm_yday = 0;
    tm.tm_isdst = 0;
    return &tm;
}

char *ctime(const long *timep)
{
    static char text[] = "Thu Jan  1 00:00:00 1970\n";
    (void)timep;
    return text;
}

struct tm *localtime(const long *timep)
{
    (void)timep;
    return epoch_tm();
}

struct tm *gmtime(const long *timep)
{
    (void)timep;
    return epoch_tm();
}

long mktime(struct tm *tm)
{
    (void)tm;
    return 0;
}

double difftime(long time1, long time0)
{
    return (double)(time1 - time0);
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    const char *stamp = "1970-01-01T00:00:00.000+0000";
    size_t n;
    (void)format;
    (void)tm;

    n = strlen(stamp);
    if (max == 0)
        return 0;
    if (n >= max)
    {
        if (s)
            s[0] = '\0';
        return 0;
    }
    memcpy(s, stamp, n + 1);
    return n;
}

int atexit(void (*function)(void))
{
    (void)function;
    return 0;
}

int isatty(int fd)
{
    if (fd >= 0 && fd <= 2)
        return 1;
    return 0;
}

void (*signal(int signum, void (*handler)(int)))(int)
{
    (void)signum;
    return handler;
}

int tcgetattr(int fd, struct termios *termios_p)
{
    (void)fd;
    if (termios_p)
    {
        memset(termios_p, 0, sizeof(*termios_p));
        termios_p->c_lflag = tty_lflag;
    }
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    if (termios_p)
        tty_lflag = termios_p->c_lflag;
    return 0;
}

int ioctl(int fd, unsigned long request, void *arg)
{
    if (request == TIOCGWINSZ && arg)
    {
        struct winsize *ws = arg;
        ws->ws_row = 24;
        ws->ws_col = 80;
        ws->ws_xpixel = 0;
        ws->ws_ypixel = 0;
        return 0;
    }
    if (request == TIOCINQ)
        return (int)syscall(SYS_ioctl, (uint64)fd, (uint64)request, (uint64)arg);
    return -1;
}

int iscntrl(int c)
{
    return (c >= 0 && c < 32) || c == 127;
}

int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int isalpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

int isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int isblank(int c)
{
    return c == ' ' || c == '\t';
}

int isprint(int c)
{
    return c >= 32 && c < 127;
}

int isgraph(int c)
{
    return c > 32 && c < 127;
}

int ispunct(int c)
{
    return isgraph(c) && !isalnum(c);
}

int isxdigit(int c)
{
    return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int toupper(int c)
{
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

size_t mbstowcs(int *dest, const char *src, size_t n)
{
    size_t len = 0;

    if (!src)
        return (size_t)-1;
    if (!dest)
    {
        while (src[len])
            len++;
        return len;
    }
    while (len < n && src[len])
    {
        dest[len] = (unsigned char)src[len];
        len++;
    }
    if (len < n)
        dest[len] = 0;
    return len;
}

size_t wcstombs(char *dest, const int *src, size_t n)
{
    size_t len = 0;

    if (!src)
        return (size_t)-1;
    if (!dest)
    {
        while (src[len])
            len++;
        return len;
    }
    while (len < n && src[len])
    {
        int c = src[len];
        dest[len] = (c >= 0 && c <= 255) ? (char)c : '?';
        len++;
    }
    if (len < n)
        dest[len] = '\0';
    return len;
}

size_t wcslen(const int *s)
{
    size_t n = 0;
    while (s && s[n])
        n++;
    return n;
}

int wcscoll(const int *a, const int *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *a - *b;
}

size_t mbrtowc(int *pwc, const char *s, size_t n, void *ps)
{
    (void)ps;
    if (!s)
        return 0;
    if (n == 0)
        return (size_t)-2;
    if (*s == '\0')
    {
        if (pwc)
            *pwc = 0;
        return 0;
    }
    if (pwc)
        *pwc = (unsigned char)*s;
    return 1;
}

size_t wcrtomb(char *s, int wc, void *ps)
{
    (void)ps;
    if (!s)
        return 1;
    *s = (wc >= 0 && wc <= 255) ? (char)wc : '?';
    return 1;
}

int btowc(int c)
{
    return c == EOF ? -1 : (unsigned char)c;
}

int wctob(int c)
{
    return (c >= 0 && c <= 255) ? c : EOF;
}

int iswalnum(int c)
{
    return isalnum(c);
}

int iswalpha(int c)
{
    return isalpha(c);
}

int iswblank(int c)
{
    return isblank(c);
}

int iswcntrl(int c)
{
    return iscntrl(c);
}

int iswdigit(int c)
{
    return isdigit(c);
}

int iswgraph(int c)
{
    return isgraph(c);
}

int iswlower(int c)
{
    return islower(c);
}

int iswprint(int c)
{
    return isprint(c);
}

int iswpunct(int c)
{
    return ispunct(c);
}

int iswspace(int c)
{
    return isspace(c);
}

int iswupper(int c)
{
    return isupper(c);
}

int iswxdigit(int c)
{
    return isxdigit(c);
}

int towlower(int c)
{
    return tolower(c);
}

int towupper(int c)
{
    return toupper(c);
}

unsigned long wctype(const char *property)
{
    if (strcmp(property, "alnum") == 0)
        return 1;
    if (strcmp(property, "alpha") == 0)
        return 2;
    if (strcmp(property, "blank") == 0)
        return 3;
    if (strcmp(property, "cntrl") == 0)
        return 4;
    if (strcmp(property, "digit") == 0)
        return 5;
    if (strcmp(property, "graph") == 0)
        return 6;
    if (strcmp(property, "lower") == 0)
        return 7;
    if (strcmp(property, "print") == 0)
        return 8;
    if (strcmp(property, "punct") == 0)
        return 9;
    if (strcmp(property, "space") == 0)
        return 10;
    if (strcmp(property, "upper") == 0)
        return 11;
    if (strcmp(property, "xdigit") == 0)
        return 12;
    return 0;
}

int iswctype(int c, unsigned long desc)
{
    switch (desc)
    {
    case 1:
        return iswalnum(c);
    case 2:
        return iswalpha(c);
    case 3:
        return iswblank(c);
    case 4:
        return iswcntrl(c);
    case 5:
        return iswdigit(c);
    case 6:
        return iswgraph(c);
    case 7:
        return iswlower(c);
    case 8:
        return iswprint(c);
    case 9:
        return iswpunct(c);
    case 10:
        return iswspace(c);
    case 11:
        return iswupper(c);
    case 12:
        return iswxdigit(c);
    default:
        return 0;
    }
}

char *strerror(int errnum)
{
    if (errnum == 2)
        return "No such file or directory";
    if (errnum == 5)
        return "Input/output error";
    if (errnum == 12)
        return "Out of memory";
    if (errnum == 17)
        return "File exists";
    if (errnum == 22)
        return "Invalid argument";
    if (errnum == 25)
        return "Not a tty";
    return "error";
}

DIR *opendir(const char *name)
{
    int fd = open(name, O_RDONLY);
    DIR *dir;

    if (fd < 0)
        return NULL;
    dir = malloc(sizeof(DIR));
    if (!dir)
    {
        close(fd);
        return NULL;
    }
    dir->fd = fd;
    memset(&dir->ent, 0, sizeof(dir->ent));
    return dir;
}

struct dirent *readdir(DIR *dirp)
{
    struct myos_dirent raw;

    if (!dirp)
        return NULL;
    while (read(dirp->fd, &raw, sizeof(raw)) == (ssize_t)sizeof(raw))
    {
        if (raw.inum == 0)
            continue;
        dirp->ent.d_ino = raw.inum;
        memcpy(dirp->ent.d_name, raw.name, sizeof(dirp->ent.d_name));
        return &dirp->ent;
    }
    return NULL;
}

int closedir(DIR *dirp)
{
    int ret;

    if (!dirp)
        return -1;
    ret = close(dirp->fd);
    free(dirp);
    return ret;
}
