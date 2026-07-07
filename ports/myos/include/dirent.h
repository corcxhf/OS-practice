#ifndef _DIRENT_H
#define _DIRENT_H

#define MYOS_D_NAME_MAX 14

struct dirent {
    unsigned long d_ino;
    char d_name[MYOS_D_NAME_MAX];
};

typedef struct DIR DIR;

#ifdef __cplusplus
extern "C" {
#endif

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#ifdef __cplusplus
}
#endif

#endif
