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
    expect_int("fstat", fstat(fd, &st), 0);
    expect_int("fstat-type", st.st_mode, T_FILE);
    expect_int("fstat-size", st.st_size, 6);
    close(fd);

    expect_int("access-existing", access("gs_raw", R_OK), 0);
    expect_int("rename", rename("gs_raw", "gs_ren"), 0);
    expect_int("access-old-missing", access("gs_raw", R_OK), -1);
    expect_int("stat-renamed", stat("gs_ren", &st), 0);
    expect_int("stat-renamed-size", st.st_size, 6);

    errno = 0;
    fd = open("gs_missing", O_RDONLY);
    expect_int("open-missing", fd, -1);
    expect_int("errno-missing", errno, ENOENT);

    expect_int("unlink-renamed", unlink("gs_ren"), 0);
    expect_int("access-unlinked", access("gs_ren", R_OK), -1);
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
    test_pipe_fork_wait();
    test_exec_redirect();

    remove("gs_stdio");
    remove("gs_raw");
    remove("gs_ren");
    remove("gs_exec");

    if (failures)
    {
        printf("GCC_STATIC_FAIL %d\n", failures);
        return 1;
    }

    printf("GCC_STATIC_PASS\n");
    return 0;
}
