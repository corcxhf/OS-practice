#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#define TIOCGWINSZ 0x5413
#define TIOCINQ 0x541B

struct winsize
{
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#ifdef __cplusplus
extern "C" {
#endif

int ioctl(int fd, unsigned long request, void *arg);

#ifdef __cplusplus
}
#endif

#endif
