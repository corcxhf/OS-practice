#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LINE_CAP 512

static int read_line(int fd, char *buf, int cap, int *truncated)
{
    int pos = 0;
    char ch;

    *truncated = 0;
    for (;;)
    {
        ssize_t n = read(fd, &ch, 1);

        if (n < 0)
            return -1;
        if (n == 0)
        {
            if (pos == 0)
                return 0;
            buf[pos] = 0;
            return 1;
        }
        if (ch == '\r')
            continue;
        if (ch == '\n')
        {
            buf[pos] = 0;
            return 1;
        }
        if (pos < cap - 1)
            buf[pos++] = ch;
        else
            *truncated = 1;
    }
}

static void print_difference(const char *left, const char *right, int line,
                             const char *left_line, const char *right_line)
{
    printf("%s %s differ: line %d\n", left, right, line);
    printf("- %s\n", left_line ? left_line : "<EOF>");
    printf("+ %s\n", right_line ? right_line : "<EOF>");
}

static int diff_files(const char *left, const char *right)
{
    char lbuf[LINE_CAP];
    char rbuf[LINE_CAP];
    int lfd;
    int rfd;
    int line = 1;

    lfd = open(left, O_RDONLY);
    if (lfd < 0)
    {
        printf("diff2: cannot open %s\n", left);
        return 1;
    }

    rfd = open(right, O_RDONLY);
    if (rfd < 0)
    {
        printf("diff2: cannot open %s\n", right);
        close(lfd);
        return 1;
    }

    for (;;)
    {
        int ltrunc;
        int rtrunc;
        int lr = read_line(lfd, lbuf, sizeof(lbuf), &ltrunc);
        int rr = read_line(rfd, rbuf, sizeof(rbuf), &rtrunc);

        if (lr < 0)
        {
            printf("diff2: read error %s\n", left);
            close(lfd);
            close(rfd);
            return 1;
        }
        if (rr < 0)
        {
            printf("diff2: read error %s\n", right);
            close(lfd);
            close(rfd);
            return 1;
        }
        if (lr == 0 && rr == 0)
            break;
        if (lr != rr || ltrunc != rtrunc || strcmp(lbuf, rbuf) != 0)
        {
            print_difference(left, right, line, lr ? lbuf : 0, rr ? rbuf : 0);
            close(lfd);
            close(rfd);
            return 1;
        }
        line++;
    }

    close(lfd);
    close(rfd);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("usage: diff2 FILE1 FILE2\n");
        return 1;
    }

    return diff_files(argv[1], argv[2]);
}
