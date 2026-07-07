#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int copy_fd(int fd, const char *name)
{
    char buf[512];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        ssize_t written = 0;

        while (written < n)
        {
            ssize_t m = write(STDOUT_FILENO, buf + written, n - written);
            if (m <= 0)
            {
                printf("cat2: write error\n");
                return -1;
            }
            written += m;
        }
    }

    if (n < 0)
    {
        if (name)
            printf("cat2: read error %s\n", name);
        else
            printf("cat2: read error\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int status = 0;
    int i;

    if (argc == 1)
        return copy_fd(STDIN_FILENO, 0) < 0 ? 1 : 0;

    for (i = 1; i < argc; i++)
    {
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0)
        {
            printf("cat2: cannot open %s\n", argv[i]);
            status = 1;
            continue;
        }
        if (copy_fd(fd, argv[i]) < 0)
            status = 1;
        close(fd);
    }

    return status;
}
