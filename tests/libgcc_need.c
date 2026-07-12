#include <stdio.h>

static unsigned long long fold_arg(const char *s)
{
    unsigned long long x = 0;

    while (s && *s)
        x = x * 131u + (unsigned char)*s++;
    return x;
}

int main(int argc, char **argv)
{
    unsigned long long seed = 123456789012345ULL + (unsigned long long)argc;
    unsigned __int128 wide;
    unsigned long long q;

    if (argc > 1)
        seed += fold_arg(argv[1]);

    wide = (unsigned __int128)seed * 1000000000000000003ULL + 987654321ULL;
    q = (unsigned long long)(wide / (97u + (unsigned)argc));
    printf("LIBGCC:%llu\n", q % 1000ULL);
    return 0;
}
