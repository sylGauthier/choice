#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#ifdef _XOPEN_SOURCE
#include <wchar.h>
#include <locale.h>
#endif
#include "term.h"

struct Entry {
    char *key, *val;
    unsigned int snum;
    int enabled;
};

static int winch = 0;
static void sigwinch(int unused) {
    signal(SIGWINCH, sigwinch);
    winch = 1;
}

static void usage(const char* prog) {
    printf("Usage: %s [-t timeout] [-e index] [-S search] [-r format] [-d format] [-s separators] [-R] [-W width] [-H height]\n", prog);
}

static void print_statusbar(int timeout, const char* searchstring, unsigned int start, unsigned int end, unsigned int total) {
    char buffer[128];
    int n;
    unsigned int r = cols - 1;

    cursor_pos(1, lines);
    erase_in_line(2);
    if (timeout > 0) {
        if ((n = sprintf(buffer, "Timeout %d", timeout)) > 0) {
            if ((unsigned int)n >= r) n = r;
            tty_write(buffer, n);
            r -= n;
        }
    }
    if (searchstring) {
        if (r) {
            tty_write("/", 1);
            r--;
        }
        n = strlen(searchstring);
        if ((unsigned int)n >= r) n = r;
        tty_write(searchstring, n);
        r -= n;
    }
    if (end > total) {
        end = total;
    }
    if ((n = sprintf(buffer, "%u-%u/%u", start, end, total)) > 0 && (unsigned int)n < r) {
        cursor_pos(cols - (unsigned int)n, lines);
        tty_write(buffer, n);
    }
}

static int format_entry(const struct Entry* entry, const char* format, int r) {
    char buffer[32];
    unsigned int p = 1, cw;
    const char *str = format, *start, *end;
    int fd = r ? STDOUT_FILENO : tty;
    int dots = 1;

    do {
        format = NULL;
        while (*str) {
            if (*str == 0x1B) {
                if (write(fd, str++, 1) != 1) return 0;
                if (*str == '[') {
                    if (write(fd, str++, 1) != 1) return 0;
                    while (*str >= 0x30 && *str <= 0x3F) if (write(fd, str++, 1) != 1) return 0;
                    while (*str >= 0x20 && *str <= 0x2F) if (write(fd, str++, 1) != 1) return 0;
                    if (*str >= 0x40 && *str <= 0x7E) if (write(fd, str++, 1) != 1) return 0;
                }
            } else if (!format && *str == '%') {
                switch (*++str) {
                    case 'k': format = str + 1; str = entry->key; break;
                    case 'v': format = str + 1; str = entry->val; break;
                }
            } else if (r) {
                if (write(fd, str++, 1) != 1) return 0;
            } else {
                if (*str == '\t') {
                    start = "    ";
                    end = start + (cw = ((4 - (p % 4)) % 4));
                    str++;
                } else {
                    unsigned long u = (unsigned char)(*str);
                    int tmp = 7;
                    while ((u & (1UL << tmp)) && tmp >= 0) u &= ~(1UL << tmp--);
                    for (start = str++; *str && (((unsigned char)(*str)) & 0xC0) == 0x80; str++) u = (u << 6) | (((unsigned char)(*str)) & 0x3F);
                    end = str;
#ifdef _XOPEN_SOURCE
                    tmp = wcwidth(u);
                    cw = (tmp < 0) ? (end - start) : tmp;
#else
                    ((void)u);
                    cw = (end - start);
#endif
                }
                if (p + cw > cols) {
                    if (dots && cols >= p) {
                        int tmp = cols - p;
                        if (tmp > 2) tmp = 2;
                        sprintf(buffer, "\x1B[%dD...", 2 - tmp);
                        tmp = cols;
                        if (tmp > 3) tmp = 3;
                        tmp += 4;
                        if (write(fd, buffer, tmp) != tmp) return 0;
                        dots = 0;
                    }
                    p = cols;
                } else {
                    if (write(fd, start, end - start) != (end - start)) return 0;
                    p += cw;
                }
            }
        }
        if (format) {
            str = format;
        }
    } while (format);
    return 1;
}

static void disp_entry(const struct Entry* entry, const char* format, unsigned int line, int selected) {
    cursor_pos(1, line);
    if (selected) color_negative();
    format_entry(entry, format, 0);
    if (selected) color_positive();
}

static void change_entry(const struct Entry* eold, const struct Entry* enew, const char* format, unsigned int lold, unsigned int lnew) {
    disp_entry(eold, format, lold, 0);
    disp_entry(enew, format, lnew, 1);
}

static void disp_page(const struct Entry* entries, unsigned int numEntries, unsigned int offset, const char* format, unsigned int selected) {
    unsigned int line;

    erase_display(2);
    for (line = 1; line < lines; line++) {
        while (offset < numEntries && !entries[offset].enabled) offset++;
        if (offset < numEntries) {
            disp_entry(entries + offset, format, line, offset == selected);
            offset++;
        } else {
            break;
        }
    }
}

#define GET_INT(var) if (++i < argc) {var = strtol(argv[i], NULL, 10); continue;} break;
#define GET_UINT(var) if (++i < argc) {var = strtoul(argv[i], NULL, 10); continue;} break;
#define GET_STR(var) if (++i < argc) {var = argv[i]; continue;} break;

int main(int argc, char** argv) {
    char buffer[2048];
    static const char* options[][2] = {
        {"--timeout", "-t"},
        {"--default-entry", "-e"},
        {"--rformat", "-r"},
        {"--dformat", "-d"},
        {"--separator", "-s"},
        {"--realtime", "-R"},
        {"--width", "-W"},
        {"--height", "-H"},
        {"--help", "-h"}
    };
    int timeout = -1, realtime = 0;
    const char *rformat = "%k\n", *dformat = "%v", *separator = " ", *searcharg = NULL, *arg;
    char *ptr, *end;
    int i, ret = 0;
    size_t size;
    struct Entry *entries = NULL, *entry;
    unsigned int j, numEntries = 0, offset, selected = 0, saved, searchlen, etotal, width = -1, height = -1;

#ifdef _XOPEN_SOURCE
    setlocale(LC_CTYPE, "");
#endif

    for (i = 1; i < argc; i++) {
        arg = argv[i];
        for (j = 0; j < sizeof(options) / sizeof(*options); j++) {
            if (!strcmp(arg, options[j][0])) {
                arg = options[j][1];
                break;
            }
        }
        if (*arg == '-') {
            while (*++arg) {
                switch (*arg) {
                    case 't': GET_INT(timeout);
                    case 'e': GET_UINT(selected);
                    case 'S': GET_STR(searcharg);
                    case 'r': GET_STR(rformat);
                    case 'd': GET_STR(dformat);
                    case 's': GET_STR(separator);
                    case 'R': realtime = 1; continue;
                    case 'W': GET_UINT(width);
                    case 'H': GET_UINT(height);
                    case 'h': usage(argv[0]); return 0;
                }
                break;
            }
        }
        if (*arg) {
            fprintf(stderr, "Error: invalid option\n");
            usage(argv[0]);
            return 1;
        }
    }

    etotal = 0;
    while (numEntries < ((unsigned int)-1) && fgets(buffer, sizeof(buffer), stdin)) {
        if (!(entry = realloc(entries, (numEntries + 1) * sizeof(*entry)))) {
            ret = 1;
            break;
        }
        entries = entry;
        entry += numEntries++;
        for (ptr = buffer; *ptr && !strchr(separator, *ptr); ptr++);
        if (*ptr) {
            *ptr++ = 0;
        } else {
            ptr = buffer;
        }
        for (end = ptr; *end && *end != '\n'; end++);
        *end = 0;
        size = (end - buffer) + 1;
        if (!(entry->key = malloc(size))) {
            ret = 1;
            break;
        }
        memcpy(entry->key, buffer, size);
        entry->val = entry->key + (ptr - buffer);
        entry->snum = etotal;
        entry->enabled = !searcharg || (strstr(entry->val, searcharg) != NULL);
        etotal += entry->enabled;
    }
    if (!numEntries) {
        fprintf(stderr, "Error: no entries\n");
        ret = 1;
    } else if (selected >= numEntries) {
        selected = numEntries - 1;
    }

    if (!ret && !term_init()) {
        fprintf(stderr, "Error: failed to init term\n");
        ret = 1;
    }

    if (!ret) {
        struct timeval t;
        int running = 1;
        long key;
        t.tv_sec = 1;
        t.tv_usec = 0;

        if (!entries[selected].enabled) {
            unsigned int before = selected, after = selected;
            while (++after < numEntries && !entries[after].enabled);
            while (before > 0 && !entries[--before].enabled);
            switch (2 * (after < numEntries) + (entries[before].enabled != 0)) {
                case 3:
                    if (after - selected < selected - before) {
                        selected = after;
                    } else {
                        selected = before;
                    }
                    break;
                case 2:
                    selected = after;
                    break;
                case 1:
                    selected = before;
                    break;
            }
        }
        if (lines <= 1) {
            offset = selected;
        } else {
            offset = (selected / (lines - 1)) * (lines - 1);
        }
        if (offset >= numEntries) {
            offset = numEntries - 1;
        }

        sigwinch(0);
        cursor_hide();
        alternate_screen();

        if (searcharg) {
            size_t n = strlen(searcharg);
            if (n >= sizeof(buffer)) n = (sizeof(buffer) - 1);
            memcpy(buffer, searcharg, n);
            searchlen = n;
        } else {
            searchlen = 0;
        }
        buffer[searchlen] = 0;
        if (realtime) {
            format_entry(entries + selected, rformat, 1);
        }

        while (running) {
            if (winch) {
                update_winsize();
                if (cols > width) cols = width;
                if (lines > height) lines = height;
                disp_page(entries, numEntries, offset, dformat, selected);
                print_statusbar(timeout, buffer[0] ? buffer : NULL, entries[offset].snum + 1, entries[offset].snum + lines - 1, etotal);
                winch = 0;
            }
            switch (key = get_key(&t)) {
                case KEY_ERROR:
                    fprintf(stderr, "Warning: failed to get key\n");
                    break;

                case KEY_TIMEOUT:
                    if (timeout > 0) {
                        if (--timeout) {
                            print_statusbar(timeout, NULL, offset + 1, offset + lines - 1, numEntries);
                        } else {
                            running = 0;
                        }
                    }
                    break;

                case 21:
                    j = lines / 2;
                    goto scroll_up;

                case 25:
                    key = KEY_UP;
                case KEY_UP:
                    saved = selected;
                    while (selected > 0 && !entries[--selected].enabled);
                    if (entries[selected].enabled) {
                        change_entry(entries + saved, entries + selected, dformat, entries[saved].snum - entries[offset].snum + 1, entries[selected].snum - entries[offset].snum + 1);
                        if (realtime && selected != saved) {
                            format_entry(entries + selected, rformat, 1);
                        }
                    } else {
                        selected = saved;
                    }
                    if (offset <= selected) {
                        break;
                    }

                case KEY_PGUP:
                    j = lines;
scroll_up:
                    saved = offset;
                    while (offset > 0 && j > 1) {
                        j -= entries[--offset].enabled;
                        if (entries[offset].enabled) saved = offset;
                    }
                    if (!entries[offset].enabled) offset = saved;
                    if (key != KEY_UP) {
                        if (realtime && selected != offset) {
                            format_entry(entries + offset, rformat, 1);
                        }
                        selected = offset;
                    }
                    disp_page(entries, numEntries, offset, dformat, selected);
                    break;

                case KEY_DOWN:
                case 5:
                    saved = selected;
                    while (selected + 1 < numEntries && !entries[++selected].enabled);
                    if (entries[selected].enabled) {
                        change_entry(entries + saved, entries + selected, dformat, entries[saved].snum - entries[offset].snum + 1, entries[selected].snum - entries[offset].snum + 1);
                        if (realtime && selected != saved) {
                            format_entry(entries + selected, rformat, 1);
                        }
                    } else {
                        selected = saved;
                    }
                    if (entries[offset].snum + lines - 1 <= entries[selected].snum) {
                        offset = selected;
                        disp_page(entries, numEntries, offset, dformat, selected);
                    }
                    break;

                case 4:
                    j = lines / 2;
                    goto scroll_down;

                case KEY_PGDN:
                    j = lines;
scroll_down:
                    saved = offset;
                    while (offset + 1 < numEntries && j > 1) {
                        j -= entries[++offset].enabled;
                        if (entries[offset].enabled) saved = offset;
                    }
                    if (!entries[offset].enabled) offset = saved;
                    if (realtime && selected != offset) {
                        format_entry(entries + offset, rformat, 1);
                    }
                    selected = offset;
                    disp_page(entries, numEntries, offset, dformat, selected);
                    break;

                case KEY_ORIG:
                    offset = 0;
                    while (!entries[offset].enabled && ++offset < numEntries);
                    if (realtime && selected != offset) {
                        format_entry(entries + offset, rformat, 1);
                    }
                    selected = offset;
                    disp_page(entries, numEntries, offset, dformat, selected);
                    break;

                case KEY_END: case KEY_END2:
                    offset = numEntries - 1;
                    while (!entries[offset].enabled && offset > 0) offset--;
                    if (realtime && selected != offset) {
                        format_entry(entries + offset, rformat, 1);
                    }
                    selected = offset;
                    disp_page(entries, numEntries, offset, dformat, selected);
                    break;

                case KEY_ESC:
                    if (searchlen) {
                        buffer[searchlen = 0] = 0;
                        goto update_search_back;
                    }

                case 3: /* <C-c> */
                    ret = 2;
                    running = 0;
                    break;

                case '\n':
                    if (entries[selected].enabled) {
                        running = 0;
                    }
                    break;

                case KEY_BACK:
                    if (searchlen) {
                        buffer[--searchlen] = 0;
                    }
update_search_back:
                    etotal = 0;
                    offset = 0;
                    for (j = 0; j < numEntries; j++) {
                        if (entries[j].enabled || (entries[j].enabled = strstr(entries[j].val, buffer) != NULL)) {
                            entries[j].snum = etotal++;
                        }
                        if (!entries[offset].enabled) {
                            offset = j;
                        }
                    }
                    if (realtime && selected != offset) {
                        format_entry(entries + offset, rformat, 1);
                    }
                    selected = offset;
                    disp_page(entries, numEntries, offset, dformat, selected);
                    break;

                default:
                    if (key >= 0x20 && key < 0x7E) {
                        if (searchlen < sizeof(buffer) - 1) {
                            buffer[searchlen++] = key;
                            buffer[searchlen] = 0;
                        }
                        etotal = 0;
                        offset = 0;
                        for (j = 0; j < numEntries; j++) {
                            if (entries[j].enabled) {
                                if ((entries[j].enabled = strstr(entries[j].val, buffer) != NULL)) {
                                    entries[j].snum = etotal++;
                                }
                                if (!entries[offset].enabled) {
                                    offset = j;
                                }
                            }
                        }
                        if (realtime && selected != offset) {
                            format_entry(entries + offset, rformat, 1);
                        }
                        selected = offset;
                        disp_page(entries, numEntries, offset, dformat, selected);
                    }
            }
            if (key > 0) {
                print_statusbar(timeout = -1, buffer[0] ? buffer : NULL, entries[offset].snum + 1, entries[offset].snum + lines - 1, etotal);
            }
        }

        erase_display(2);
        color_positive();
        cursor_pos(1, 1);
        cursor_show();
        normal_screen();
    }

    if (!ret && !realtime) {
        ret = !format_entry(entries + selected, rformat, 1);
    }

    for (j = 0; j < numEntries; j++) {
        free(entries[j].key);
    }
    free(entries);
    return ret;
}
