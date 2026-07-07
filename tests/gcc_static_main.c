#include "gcc_static_lib.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define T_FILE 2

static int failures;

static void fail(const char *name)
{
    printf("GCC_STATIC_FAIL %s\n", name);
    failures++;
}

static void expect_int(const char *name, long got, long want)
{
    if (got != want)
    {
        printf("GCC_STATIC_FAIL %s got=%ld want=%ld\n", name, got, want);
        failures++;
    }
}

static void expect_str(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) != 0)
    {
        printf("GCC_STATIC_FAIL %s got=[%s] want=[%s]\n", name, got, want);
        failures++;
    }
}

static int read_file(const char *path, char *buf, int cap)
{
    int fd = open(path, O_RDONLY);
    int n;

    if (fd < 0)
        return -1;
    memset(buf, 0, cap);
    n = read(fd, buf, cap - 1);
    close(fd);
    return n;
}

static void test_multi_object_and_argv(int argc, char **argv)
{
    char reversed[16];

    expect_int("argc", argc, 3);
    if (argc >= 3)
    {
        expect_str("argv1", argv[1], "alpha");
        expect_str("argv2", argv[2], "beta");
    }
    expect_int("word-count", gcc_static_count_words("one two\tthree\nfour"), 4);
    gcc_static_reverse_copy(reversed, "MyOS");
    expect_str("reverse-copy", reversed, "SOyM");

    if (argc >= 3)
        expect_int("weighted-argv", gcc_static_weighted_arg_len(argc, argv), 35);
}

static void test_stdio_files(void)
{
    FILE *f;
    char buf[32];

    remove("gs_stdio");
    f = fopen("gs_stdio", "w");
    if (!f)
    {
        fail("fopen-write");
        return;
    }
    expect_int("fwrite", fwrite("alpha\nbeta\n", 1, 11, f), 11);
    expect_int("fclose-write", fclose(f), 0);

    f = fopen("gs_stdio", "r");
    if (!f)
    {
        fail("fopen-read");
        return;
    }
    memset(buf, 0, sizeof(buf));
    expect_int("fread", fread(buf, 1, sizeof(buf) - 1, f), 11);
    expect_int("fclose-read", fclose(f), 0);
    expect_str("stdio-content", buf, "alpha\nbeta\n");
}

static void test_sys_files(void)
{
    struct stat st;
    char buf[32];
    char cwd[64];
    int fd;

    remove("gs_raw");
    remove("gs_ren");

    fd = open("gs_raw", O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        fail("open-raw");
        return;
    }
    expect_int("write-raw", write(fd, "abcdef", 6), 6);
    expect_int("seek-set", lseek(fd, 2, SEEK_SET), 2);
    expect_int("write-overwrite", write(fd, "ZZ", 2), 2);
    expect_int("seek-rewind", lseek(fd, 0, SEEK_SET), 0);
    memset(buf, 0, sizeof(buf));
    expect_int("read-raw", read(fd, buf, 6), 6);
    expect_str("raw-content", buf, "abZZef");
    expect_int("truncate-mid", ftruncate(fd, 4), 0);
    expect_int("seek-after-trunc", lseek(fd, 0, SEEK_SET), 0);
    memset(buf, 0, sizeof(buf));
    expect_int("read-truncated", read(fd, buf, 6), 4);
    expect_str("truncated-content", buf, "abZZ");
    expect_int("truncate-extend", ftruncate(fd, 8), 0);
    expect_int("seek-after-extend", lseek(fd, 0, SEEK_SET), 0);
    memset(buf, 0x7f, sizeof(buf));
    expect_int("read-extended", read(fd, buf, 8), 8);
    if (memcmp(buf, "abZZ\0\0\0\0", 8) != 0)
        fail("truncate-extend-zero-fill");
    expect_int("fstat", fstat(fd, &st), 0);
    expect_int("fstat-type", st.st_mode, T_FILE);
    expect_int("fstat-size", st.st_size, 8);
    close(fd);

    expect_int("access-existing", access("gs_raw", R_OK), 0);
    expect_int("rename", rename("gs_raw", "gs_ren"), 0);
    expect_int("access-old-missing", access("gs_raw", R_OK), -1);
    expect_int("stat-renamed", stat("gs_ren", &st), 0);
    expect_int("stat-renamed-size", st.st_size, 8);
    expect_int("fstatat-renamed", fstatat(AT_FDCWD, "gs_ren", &st, 0), 0);
    expect_int("fstatat-size", st.st_size, 8);

    expect_int("mkdir", mkdir("gs_dir", 0777), 0);
    expect_int("chdir-dir", chdir("gs_dir"), 0);
    if (!getcwd(cwd, sizeof(cwd)))
        fail("getcwd");
    else
        expect_str("getcwd-dir", cwd, "/gs_dir");
    expect_int("chdir-parent", chdir(".."), 0);
    if (!getcwd(cwd, sizeof(cwd)))
        fail("getcwd-parent");
    else
        expect_str("getcwd-parent", cwd, "/");

    errno = 0;
    fd = open("gs_missing", O_RDONLY);
    expect_int("open-missing", fd, -1);
    expect_int("errno-missing", errno, ENOENT);

    expect_int("unlink-renamed", unlink("gs_ren"), 0);
    expect_int("access-unlinked", access("gs_ren", R_OK), -1);
    expect_int("unlink-dir", unlink("gs_dir"), 0);
}

static void test_open_flags(void)
{
    char buf[32];
    int fd;

    remove("gs_flags");
    fd = open("gs_flags", O_WRONLY | O_CREAT | O_EXCL);
    if (fd < 0)
    {
        fail("open-excl-create");
        return;
    }
    expect_int("open-excl-write", write(fd, "first", 5), 5);
    close(fd);

    errno = 0;
    fd = open("gs_flags", O_WRONLY | O_CREAT | O_EXCL);
    expect_int("open-excl-existing", fd, -1);
    expect_int("open-excl-errno", errno, EEXIST);

    fd = open("gs_flags", O_WRONLY | O_APPEND);
    if (fd < 0)
    {
        fail("open-append");
        return;
    }
    expect_int("append-write", write(fd, "+tail", 5), 5);
    close(fd);

    if (read_file("gs_flags", buf, sizeof(buf)) < 0)
    {
        fail("append-read");
        return;
    }
    expect_str("append-content", buf, "first+tail");
    expect_int("unlink-flags", unlink("gs_flags"), 0);
}

static void test_pipe_fork_wait(void)
{
    int fds[2];
    int pid;
    int status = -1;
    char buf[16];

    if (pipe(fds) < 0)
    {
        fail("pipe");
        return;
    }

    pid = fork();
    if (pid < 0)
    {
        fail("fork");
        return;
    }

    if (pid == 0)
    {
        close(fds[0]);
        write(fds[1], "pipe-ok", 7);
        close(fds[1]);
        exit(7);
    }

    close(fds[1]);
    memset(buf, 0, sizeof(buf));
    expect_int("pipe-read", read(fds[0], buf, sizeof(buf) - 1), 7);
    close(fds[0]);
    expect_str("pipe-content", buf, "pipe-ok");
    expect_int("waitpid", waitpid(pid, &status, 0), pid);
    expect_int("child-status", status, 7);
}

static void test_dup2_redirect(void)
{
    int saved;
    int fd;
    char buf[32];

    remove("gs_dup2");
    saved = dup(STDOUT_FILENO);
    if (saved < 0)
    {
        fail("dup-save-stdout");
        return;
    }
    fd = open("gs_dup2", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        close(saved);
        fail("dup2-open");
        return;
    }

    expect_int("dup2-to-stdout", dup2(fd, STDOUT_FILENO), STDOUT_FILENO);
    close(fd);
    expect_int("dup2-write", write(STDOUT_FILENO, "dup2-ok", 7), 7);
    expect_int("dup2-restore", dup2(saved, STDOUT_FILENO), STDOUT_FILENO);
    close(saved);

    if (read_file("gs_dup2", buf, sizeof(buf)) < 0)
    {
        fail("dup2-read");
        return;
    }
    expect_str("dup2-content", buf, "dup2-ok");
}

static void test_exec_redirect(void)
{
    char *child_argv[] = {"/bin/gcc-hello", 0};
    int pid;
    int status = -1;
    char buf[64];

    remove("gs_exec");
    pid = fork();
    if (pid < 0)
    {
        fail("exec-fork");
        return;
    }

    if (pid == 0)
    {
        int fd;

        close(STDOUT_FILENO);
        fd = open("gs_exec", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd != STDOUT_FILENO)
            exit(90);
        execvp(child_argv[0], child_argv);
        exit(91);
    }

    expect_int("exec-wait", waitpid(pid, &status, 0), pid);
    expect_int("exec-status", status, 0);
    if (read_file("gs_exec", buf, sizeof(buf)) < 0)
    {
        fail("exec-output-open");
        return;
    }
    expect_str("exec-output", buf, "HELLO_USERLAND\n");
}

int main(int argc, char **argv)
{
    test_multi_object_and_argv(argc, argv);
    test_stdio_files();
    test_sys_files();
    test_open_flags();
    test_pipe_fork_wait();
    test_dup2_redirect();
    test_exec_redirect();

    remove("gs_stdio");
    remove("gs_raw");
    remove("gs_ren");
    remove("gs_flags");
    remove("gs_exec");
    remove("gs_dup2");
    unlink("gs_dir");

    if (failures)
    {
        printf("GCC_STATIC_FAIL %d\n", failures);
        return 1;
    }

    printf("GCC_STATIC_PASS\n");
    return 0;
}
