#ifndef __PHABOS_TERMIOS_H__
#define __PHABOS_TERMIOS_H__

#define TCGETS      0
#define TCSETS      1
#define TSETSW      2
#define TSETSF      3

#define CSIZE       (3 << 0)
#define CBAUD       (31 << 7)

#define CS5         (0 << 0)
#define CS6         (1 << 0)
#define CS7         (2 << 0)
#define CS8         (3 << 0)
#define PARENB      (1 << 3)
#define PARODD      (1 << 4)
#define CSTOPB      (1 << 5)
#define CMSPAR      (1 << 6)
#define B0          (0 << 7)
#define B50         (1 << 7)
#define B75         (2 << 7)
#define B110        (3 << 7)
#define B134        (4 << 7)
#define B150        (5 << 7)
#define B200        (6 << 7)
#define B300        (7 << 7)
#define B600        (8 << 7)
#define B1200       (9 << 7)
#define B1800       (10 << 7)
#define B2400       (11 << 7)
#define B4800       (12 << 7)
#define B9600       (13 << 7)
#define B19200      (14 << 7)
#define B38400      (15 << 7)
#define B57600      (16 << 7)
#define B115200     (17 << 7)
#define B230400     (18 << 7)

#define TCSANOW     0
#define TCSADRAIN   1
#define TCSAFLUSH   2

typedef unsigned long tcflag_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
};

#endif /* __PHABOS_TERMIOS_H__ */

