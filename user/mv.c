#include "fs.h"
#include "proc.h"
#include "types.h"

#define STDERR_FD 2
#define COPY_CAP 512

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

static int str_len(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

static void write_str(int fd, const char *s)
{
    syscall(SYS_write, fd, (uint64)s, (uint64)str_len(s));
}

static int close_status(int fd)
{
    return (int)syscall(SYS_close, fd, 0, 0);
}

static int open_stat(const char *path, struct stat *st, int *fd_out)
{
    int fd = (int)syscall(SYS_open, (uint64)path, O_RDONLY, 0);

    if (fd < 0)
        return -1;
    if ((int)syscall(SYS_fstat, fd, (uint64)st, 0) < 0)
    {
        close_status(fd);
        return -1;
    }

    *fd_out = fd;
    return 0;
}

static int same_file(const struct stat *a, const struct stat *b)
{
    return a->dev == b->dev && a->ino == b->ino;
}

static int copy_fd_to_path(int in_fd, const char *src, const char *dst)
{
    char buf[COPY_CAP];
    int out_fd;
    int n;

    out_fd = (int)syscall(SYS_open, (uint64)dst, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (out_fd < 0)
    {
        write_str(STDERR_FD, "mv: cannot create ");
        write_str(STDERR_FD, dst);
        write_str(STDERR_FD, "\n");
        return -1;
    }

    while ((n = (int)syscall(SYS_read, in_fd, (uint64)buf, sizeof(buf))) > 0)
    {
        int written = 0;
        while (written < n)
        {
            int m = (int)syscall(SYS_write, out_fd, (uint64)(buf + written), n - written);
            if (m <= 0)
            {
                write_str(STDERR_FD, "mv: write error ");
                write_str(STDERR_FD, dst);
                write_str(STDERR_FD, "\n");
                close_status(out_fd);
                return -1;
            }
            written += m;
        }
    }

    if (n < 0)
    {
        write_str(STDERR_FD, "mv: read error ");
        write_str(STDERR_FD, src);
        write_str(STDERR_FD, "\n");
        close_status(out_fd);
        return -1;
    }

    return close_status(out_fd);
}

static int move_file(const char *src, const char *dst)
{
    struct stat src_st;
    struct stat dst_st;
    int src_fd;
    int dst_fd;
    int dst_exists;

    if (open_stat(src, &src_st, &src_fd) < 0)
    {
        write_str(STDERR_FD, "mv: cannot open ");
        write_str(STDERR_FD, src);
        write_str(STDERR_FD, "\n");
        return -1;
    }

    if (src_st.type == T_DIR)
    {
        write_str(STDERR_FD, "mv: cannot move directory ");
        write_str(STDERR_FD, src);
        write_str(STDERR_FD, "\n");
        close_status(src_fd);
        return -1;
    }

    dst_exists = open_stat(dst, &dst_st, &dst_fd) == 0;
    if (dst_exists)
    {
        close_status(dst_fd);
        if (same_file(&src_st, &dst_st))
        {
            close_status(src_fd);
            return 0;
        }
        if (dst_st.type == T_DIR)
        {
            write_str(STDERR_FD, "mv: cannot overwrite directory ");
            write_str(STDERR_FD, dst);
            write_str(STDERR_FD, "\n");
            close_status(src_fd);
            return -1;
        }
    }

    if (copy_fd_to_path(src_fd, src, dst) < 0)
    {
        close_status(src_fd);
        return -1;
    }

    close_status(src_fd);
    if ((int)syscall(SYS_unlink, (uint64)src, 0, 0) < 0)
    {
        write_str(STDERR_FD, "mv: cannot remove ");
        write_str(STDERR_FD, src);
        write_str(STDERR_FD, "\n");
        return -1;
    }

    return 0;
}

void main(int argc, char *argv[])
{
    if (argc != 3)
    {
        write_str(STDERR_FD, "usage: mv SRC DST\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    syscall(SYS_exit, move_file(argv[1], argv[2]) < 0 ? -1 : 0, 0, 0);
}

void __attribute__((naked, section(".text.entry"))) _start(void)
{
    __asm__ volatile("call main");
}
