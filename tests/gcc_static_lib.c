#include "gcc_static_lib.h"

#include <ctype.h>
#include <string.h>

int gcc_static_count_words(const char *text)
{
    int words = 0;
    int in_word = 0;

    while (*text)
    {
        if (isspace((unsigned char)*text))
        {
            in_word = 0;
        }
        else if (!in_word)
        {
            words++;
            in_word = 1;
        }
        text++;
    }

    return words;
}

int gcc_static_weighted_arg_len(int argc, char **argv)
{
    int total = 0;

    for (int i = 0; i < argc; i++)
        total += (i + 1) * (int)strlen(argv[i]);

    return total;
}

void gcc_static_reverse_copy(char *dst, const char *src)
{
    int len = (int)strlen(src);

    for (int i = 0; i < len; i++)
        dst[i] = src[len - 1 - i];
    dst[len] = 0;
}
