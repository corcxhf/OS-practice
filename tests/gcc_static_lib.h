#ifndef GCC_STATIC_LIB_H
#define GCC_STATIC_LIB_H

int gcc_static_count_words(const char *text);
int gcc_static_weighted_arg_len(int argc, char **argv);
void gcc_static_reverse_copy(char *dst, const char *src);

#endif
