#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>

static struct termios origterm;
int tty;
unsigned int cols, lines;

int update_winsize(void) {
    struct winsize win;

    if (!ioctl(tty, TIOCGWINSZ, &win) && win.ws_row > 0 && win.ws_col > 0) {
        cols = win.ws_col;
        lines = win.ws_row;
        return 1;
    }
    return 0;
}

static void term_cleanup(void) {
    tcsetattr(tty, TCSAFLUSH, &origterm);
    close(tty);
}

int term_init(void) {
    struct termios tmp;

    if ((tty = open("/dev/tty", O_RDWR | O_NONBLOCK)) == -1) {
        fprintf(stderr, "Error: failed to open /dev/tty\n");
        return 0;
    }
    if (tcgetattr(tty, &origterm) || atexit(term_cleanup)) return 0;
    tmp = origterm;
    tmp.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    return !tcsetattr(tty, TCSAFLUSH, &tmp) && update_winsize();
}

int cursor_pos(unsigned int x, unsigned int y) {
    char buffer[64];
    int n = sprintf(buffer, "\x1B[%u;%uH", y, x);
    return n > 0 && write(tty, buffer, n) == n;
}

int color_negative(void) {
    return write(tty, "\x1B[7m", 4) == 4;
}

int color_positive(void) {
    return write(tty, "\x1B[27m", 5) == 5;
}

int cursor_hide(void) {
    return write(tty, "\x1B[?25l", 6) == 6;
}

int cursor_show(void) {
    return write(tty, "\x1B[?25h", 6) == 6;
}

int alternate_screen(void) {
    return write(tty, "\x1B[?1049h", 8) == 8;
}

int normal_screen(void) {
    return write(tty, "\x1B[?1049l", 8) == 8;
}

int erase_display(int cmd) {
    char buffer[64];
    int n = sprintf(buffer, "\x1B[%dJ", cmd);
    return n > 0 && write(tty, buffer, n) == n;
}

int erase_in_line(int cmd) {
    char buffer[64];
    int n = sprintf(buffer, "\x1B[%dK", cmd);
    return n > 0 && write(tty, buffer, n) == n;
}

int tty_write(const void* buf, size_t count) {
    const char* ptr;
    ssize_t w;
    for (ptr = buf; count; count -= w) {
        if ((w = write(tty, ptr, count)) < 0) return 0;
    }
    return 1;
}

long get_key(const struct timeval* timeout) {
    struct timeval t;
    fd_set s;
    long key = -1;
    char k;

    FD_ZERO(&s);
    FD_SET(tty, &s);
    t = *timeout;
    switch (select(tty + 1, &s, NULL, NULL, &t)) {
        case 0:
            key = 0;
            break;
        case 1:
            if (read(tty, &k, 1) != -1) {
                key = k;
                if (k == 0x1B) {
                    FD_ZERO(&s);
                    FD_SET(tty, &s);
                    t.tv_sec = 0;
                    t.tv_usec = 0;
                    if (select(tty + 1, &s, NULL, NULL, &t) == 1 && read(tty, &k, 1) != -1 && k == '[') {
                        key = (key << 8) | k;
                        FD_ZERO(&s);
                        FD_SET(tty, &s);
                        t.tv_sec = 0;
                        t.tv_usec = 0;
                        if (select(tty + 1, &s, NULL, NULL, &t) == 1 && read(tty, &k, 1) != -1) {
                            key = (key << 8) | k;
                        }
                    }
                }
            }
    }
    return key;
}
