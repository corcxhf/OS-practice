#ifndef _TERMIOS_H
#define _TERMIOS_H

#define TCSAFLUSH 2

#define BRKINT 0000002
#define ICRNL 0000400
#define INPCK 0000020
#define ISTRIP 0000040
#define IXON 0002000
#define OPOST 0000001
#define CS8 0000060
#define ECHO 0000010
#define ICANON 0000002
#define IEXTEN 0100000
#define ISIG 0000001

#define VMIN 6
#define VTIME 5

struct termios
{
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_cc[32];
};

#ifdef __cplusplus
extern "C" {
#endif

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

#ifdef __cplusplus
}
#endif

#endif
