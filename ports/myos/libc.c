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
#define AT_FDCWD -100
#define _SC_PAGESIZE 30
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

typedef struct FILE
{
    int fd;
    int eof;
    int err;
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
    long st_mtime;
};

struct termios
{
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_cc[32];
};

void free(void *ptr);
void *malloc(size_t nbytes);
void *realloc(void *ptr, size_t nbytes);
long strtol(const char *nptr, char **endptr, int base);
void *memset(void *dst, int c, size_t n);
static char *copy_cstr(char *dst, size_t cap, const char *src);

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
    ret = (int)syscall(SYS_fstat, (uint64)fd, (uint64)st, 0);
    if (ret < 0)
        errno_value = 5;
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

int kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return -1;
}

int execvp(const char *file, char *const argv[])
{
    return (int)syscall(SYS_exec, (uint64)file, (uint64)argv, 0);
}

int execv(const char *path, char *const argv[])
{
    return execvp(path, argv);
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    (void)envp;
    return execvp(path, argv);
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
    (void)fd;
    (void)cmd;
    return 0;
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

char *strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
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
    {0, 0, 0, 0, {0}},
    {1, 0, 0, 0, {0}},
    {2, 0, 0, 0, {0}},
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

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    ssize_t ret;
    size_t bytes = size * nmemb;
    if (bytes == 0)
        return 0;
    ret = read(stream->fd, ptr, bytes);
    if (ret < 0)
    {
        stream->err = 1;
        return 0;
    }
    if ((size_t)ret < bytes)
        stream->eof = 1;
    return (size_t)ret / size;
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

static void out_uint(char **buf, size_t *left, int *count, unsigned long x, int base, int neg)
{
    char tmp[32];
    int n = 0;

    if (neg)
        out_char(buf, left, count, '-');

    if (x == 0)
    {
        out_char(buf, left, count, '0');
        return;
    }

    while (x)
    {
        int digit = x % (unsigned long)base;
        tmp[n++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        x /= (unsigned long)base;
    }
    while (n--)
        out_char(buf, left, count, tmp[n]);
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
        if (*fmt != '%')
        {
            out_char(&out, &left, &count, *fmt++);
            continue;
        }

        fmt++;
        while (*fmt >= '0' && *fmt <= '9')
            fmt++;
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
            out_uint(&out, &left, &count, x, 10, v < 0);
        }
        else if (*fmt == 'u')
        {
            unsigned long x = long_arg ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            out_uint(&out, &left, &count, x, 10, 0);
        }
        else if (*fmt == 'x' || *fmt == 'X')
        {
            unsigned long x = long_arg ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            out_uint(&out, &left, &count, x, 16, 0);
        }
        else if (*fmt == 'p')
        {
            out_str(&out, &left, &count, "0x");
            out_uint(&out, &left, &count, (unsigned long)va_arg(ap, void *), 16, 0);
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
        if (*fmt == 'd')
        {
            int *out = va_arg(ap, int *);
            char *end;
            long v;
            while (scan_space((unsigned char)*str))
                str++;
            v = strtol(str, &end, 10);
            if (end == str)
                break;
            *out = (int)v;
            str = end;
            count++;
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

int atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}

char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite)
{
    (void)name;
    (void)value;
    (void)overwrite;
    return 0;
}

int unsetenv(const char *name)
{
    (void)name;
    return 0;
}

int putenv(char *string)
{
    (void)string;
    return 0;
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

int getpagesize(void)
{
    return 4096;
}

void abort(void)
{
    exit(134);
}

long time(long *tloc)
{
    if (tloc)
        *tloc = 2026;
    return 2026;
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

int isprint(int c)
{
    return c >= 32 && c < 127;
}

int tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int toupper(int c)
{
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
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

char *environ[] = {NULL};

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
