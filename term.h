#ifndef TERM_H
#define TERM_H

extern int tty;
extern unsigned int cols, lines;

int term_init(void);
int update_winsize(void);

void cursor_pos(unsigned int x, unsigned int y);
void color_negative(void);
void color_positive(void);
int cursor_hide(void);
int cursor_show(void);
int alternate_screen(void);
int normal_screen(void);
int erase_display(int cmd);
void erase_in_line(int cmd);
int tty_write(const void* buf, size_t count);

#define KEY_UP    ((0x1B << 16) | ('[' << 8) | 'A')
#define KEY_DOWN  ((0x1B << 16) | ('[' << 8) | 'B')
#define KEY_RIGHT ((0x1B << 16) | ('[' << 8) | 'C')
#define KEY_LEFT  ((0x1B << 16) | ('[' << 8) | 'D')
#define KEY_END   ((0x1B << 16) | ('[' << 8) | '4')
#define KEY_END2  ((0x1B << 16) | ('[' << 8) | 'F')
#define KEY_ORIG  ((0x1B << 16) | ('[' << 8) | 'H')
#define KEY_PGUP  ((0x1B << 16) | ('[' << 8) | '5')
#define KEY_PGDN  ((0x1B << 16) | ('[' << 8) | '6')
#define KEY_ESC   (0x1B)
#define KEY_BACK  (0x7F)
#define KEY_TIMEOUT (0)
#define KEY_ERROR   (-1)
long get_key(const struct timeval* timeout);

#endif
