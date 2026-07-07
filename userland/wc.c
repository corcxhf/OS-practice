#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

struct counts
{
    long lines;
    long words;
    long bytes;
};

static int is_space_char(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

static void count_buf(struct counts *counts, const char *buf, ssize_t n, int *in_word)
{
    ssize_t i;

    counts->bytes += n;
    for (i = 0; i < n; i++)
    {
        if (buf[i] == '\n')
            counts->lines++;

        if (is_space_char(buf[i]))
        {
            *in_word = 0;
        }
        else if (!*in_word)
        {
            counts->words++;
            *in_word = 1;
        }
    }
}

static int count_fd(int fd, struct counts *counts)
{
    char buf[512];
    int in_word = 0;
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
        count_buf(counts, buf, n, &in_word);

    return n < 0 ? -1 : 0;
}

static void add_counts(struct counts *dst, const struct counts *src)
{
    dst->lines += src->lines;
    dst->words += src->words;
    dst->bytes += src->bytes;
}

static void print_counts(const struct counts *counts, const char *name)
{
    if (name)
        printf("%ld %ld %ld %s\n", counts->lines, counts->words, counts->bytes, name);
    else
        printf("%ld %ld %ld\n", counts->lines, counts->words, counts->bytes);
}

static int count_file(const char *path, struct counts *counts)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0)
    {
        printf("wc2: cannot open %s\n", path);
        return -1;
    }

    if (count_fd(fd, counts) < 0)
    {
        printf("wc2: read error %s\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    int had_error = 0;
    int i;

    if (argc == 1)
    {
        struct counts counts = {0, 0, 0};

        if (count_fd(STDIN_FILENO, &counts) < 0)
            return 1;
        print_counts(&counts, 0);
        return 0;
    }

    struct counts total = {0, 0, 0};
    for (i = 1; i < argc; i++)
    {
        struct counts counts = {0, 0, 0};

        if (count_file(argv[i], &counts) < 0)
        {
            had_error = 1;
            continue;
        }
        print_counts(&counts, argv[i]);
        add_counts(&total, &counts);
    }

    if (argc > 2)
        print_counts(&total, "total");

    return had_error ? 1 : 0;
}
