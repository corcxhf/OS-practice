#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int count_fd(int fd, const char *name)
{
    char buf[256];
    long lines = 0;
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        ssize_t i;

        for (i = 0; i < n; i++)
        {
            if (buf[i] == '\n')
                lines++;
        }
    }

    if (n < 0)
    {
        if (name)
            printf("lines: read error %s\n", name);
        else
            printf("lines: read error\n");
        return -1;
    }

    printf("%ld\n", lines);
    return 0;
}

int main(int argc, char **argv)
{
    int status = 0;
    int i;

    if (argc == 1)
        return count_fd(STDIN_FILENO, 0) < 0 ? 1 : 0;

    for (i = 1; i < argc; i++)
    {
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0)
        {
            printf("lines: cannot open %s\n", argv[i]);
            status = 1;
            continue;
        }
        if (count_fd(fd, argv[i]) < 0)
            status = 1;
        close(fd);
    }

    return status;
}
