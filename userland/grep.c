#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LINE_CAP 1024
#define READ_CAP 256

static int line_matches(const char *pattern, const char *line)
{
    return strstr(line, pattern) != 0;
}

static void print_match(const char *name, int show_name, const char *line)
{
    if (show_name)
        printf("%s:%s\n", name, line);
    else
        printf("%s\n", line);
}

static int flush_line(const char *pattern, const char *name, int show_name, char *line, int *pos)
{
    int matched;

    line[*pos] = 0;
    matched = line_matches(pattern, line);
    if (matched)
        print_match(name, show_name, line);
    *pos = 0;
    return matched;
}

static int grep_fd(int fd, const char *name, int show_name, const char *pattern)
{
    char read_buf[READ_CAP];
    char line[LINE_CAP];
    int pos = 0;
    int matched = 0;
    ssize_t n;

    while ((n = read(fd, read_buf, sizeof(read_buf))) > 0)
    {
        ssize_t i;

        for (i = 0; i < n; i++)
        {
            char c = read_buf[i];

            if (c == '\r')
                continue;
            if (c == '\n')
            {
                if (flush_line(pattern, name, show_name, line, &pos))
                    matched = 1;
                continue;
            }
            if (pos < LINE_CAP - 1)
                line[pos++] = c;
        }
    }

    if (pos > 0 && flush_line(pattern, name, show_name, line, &pos))
        matched = 1;
    return matched;
}

static int grep_file(const char *path, int show_name, const char *pattern, int *had_error)
{
    int fd = open(path, O_RDONLY);
    int matched;

    if (fd < 0)
    {
        printf("grep2: cannot open %s\n", path);
        *had_error = 1;
        return 0;
    }

    matched = grep_fd(fd, path, show_name, pattern);
    close(fd);
    return matched;
}

int main(int argc, char **argv)
{
    int matched = 0;
    int had_error = 0;

    if (argc < 2)
    {
        printf("usage: grep2 PATTERN [FILE...]\n");
        return 1;
    }

    if (argc == 2)
    {
        matched = grep_fd(STDIN_FILENO, "-", 0, argv[1]);
    }
    else
    {
        int show_name = argc > 3;
        int i;

        for (i = 2; i < argc; i++)
        {
            if (grep_file(argv[i], show_name, argv[1], &had_error))
                matched = 1;
        }
    }

    if (had_error)
        return 1;
    return matched ? 0 : 1;
}
