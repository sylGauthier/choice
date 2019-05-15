#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include "term.h"

struct Entry {
    char *key, *val;
    unsigned int num;
    int enabled;
};

static int winch = 0;
static void sigwinch(int unused) {
    signal(SIGWINCH, sigwinch);
    winch = 1;
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
    unsigned int p = 1;
    const char* str = format;
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
                char c = *str++;
                if (c == '\t') c = ' ';
                if (p >= cols) {
                    if (dots) {
                        if (write(fd, "\x1B[2D...", 7) != 7) return 0;
                        dots = 0;
                    }
                } else {
                    if (write(fd, &c, 1) != 1) return 0;
                    p++;
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
        {"--separator", "-s"}
    };
    int timeout = -1;
    const char *rformat = "%k", *dformat = "%v", *separator = " ", *arg;
    char *ptr, *end;
    int i, ret = 0;
    size_t size;
    struct Entry *entries = NULL, *entry;
    unsigned int j, numEntries = 0, offset, selected = 0, saved, searchlen, etotal;

    for (i = 1; i < argc; i++) {
        arg = argv[i];
        for (j = 0; j < sizeof(options) / sizeof(*options); j++) {
            if (!strcmp(arg, options[j][0])) {
                arg = options[j][1];
                break;
            }
        }
        if (*arg++ == '-') {
            while (*arg) {
                switch (*arg++) {
                    case 't': GET_INT(timeout);
                    case 'e': GET_UINT(selected);
                    case 'r': GET_STR(rformat);
                    case 'd': GET_STR(dformat);
                    case 's': GET_STR(separator);
                }
                break;
            }
        }
        if (*arg) {
            fprintf(stderr, "Error: invalid option\n");
            return 1;
        }
    }

    while (fgets(buffer, sizeof(buffer), stdin)) {
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
        for (end = ptr; end && *end != '\n'; end++);
        *end = 0;
        size = (end - buffer) + 1;
        if (!(entry->key = malloc(size))) {
            ret = 1;
            break;
        }
        memcpy(entry->key, buffer, size);
        entry->val = entry->key + (ptr - buffer);
        entry->num = numEntries - 1;
        entry->enabled = 1;
    }
    if (!numEntries) {
        fprintf(stderr, "Error: no entries\n");
        ret = 1;
    } else if (selected >= numEntries) {
        selected = numEntries - 1;
    }
    etotal = numEntries;
    offset = selected;

    if (!ret && !term_init()) {
        fprintf(stderr, "Error: failed to init term\n");
        ret = 1;
    }
    cursor_hide();
    alternate_screen();

    buffer[searchlen = 0] = 0;
    disp_page(entries, numEntries, offset, dformat, selected);
    if (!ret) {
        struct timeval t;
        int running = 1;
        long key;
        t.tv_sec = 1;
        t.tv_usec = 0;

        sigwinch(0);
        print_statusbar(timeout, NULL, offset + 1, offset + lines - 1, numEntries);
        while (running) {
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

                default:
                    switch (key) {
                        case KEY_UP:
                            saved = selected;
                            while (selected > 0 && !entries[--selected].enabled);
                            if (entries[selected].enabled) {
                                change_entry(entries + saved, entries + selected, dformat, entries[saved].num - entries[offset].num + 1, entries[selected].num - entries[offset].num + 1);
                            } else {
                                selected = saved;
                            }
                            if (offset > selected) {
                                j = lines - 1;
                                offset = saved = selected;
                                while (offset > 0 && j > 1) {
                                    j -= entries[--offset].enabled;
                                    if (entries[offset].enabled) saved = offset;
                                }
                                if (!entries[offset].enabled) offset = saved;
                                disp_page(entries, numEntries, offset, dformat, selected);
                            }
                            break;
                        case KEY_DOWN:
                            saved = selected;
                            while (selected + 1 < numEntries && !entries[++selected].enabled);
                            if (entries[selected].enabled) {
                                change_entry(entries + saved, entries + selected, dformat, entries[saved].num - entries[offset].num + 1, entries[selected].num - entries[offset].num + 1);
                            } else {
                                selected = saved;
                            }
                            if (entries[offset].num + lines - 1 <= entries[selected].num) {
                                offset = selected;
                                disp_page(entries, numEntries, offset, dformat, selected);
                            }
                            break;
                        case KEY_ESC:
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
                            etotal = 0;
                            offset = 0;
                            for (j = 0; j < numEntries; j++) {
                                if ((entries[j].enabled = strstr(entries[j].val, buffer) != NULL)) {
                                    entries[j].num = etotal++;
                                }
                                if (!entries[offset].enabled) {
                                    offset = j;
                                }
                            }
                            selected = offset;
                            disp_page(entries, numEntries, offset, dformat, selected);
                            break;
                        default:
                            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
                                if (searchlen < sizeof(buffer) - 1) {
                                    buffer[searchlen++] = key;
                                    buffer[searchlen] = 0;
                                }
                                etotal = 0;
                                offset = 0;
                                for (j = 0; j < numEntries; j++) {
                                    if (entries[j].enabled) {
                                        if ((entries[j].enabled = strstr(entries[j].val, buffer) != NULL)) {
                                            entries[j].num = etotal++;
                                        }
                                        if (!entries[offset].enabled) {
                                            offset = j;
                                        }
                                    }
                                }
                                selected = offset;
                                disp_page(entries, numEntries, offset, dformat, selected);
                            }
                    }
                    print_statusbar(timeout = -1, buffer[0] ? buffer : NULL, entries[offset].num + 1, entries[offset].num + lines - 1, etotal);
                    break;
            }
            if (winch) {
                update_winsize();
                disp_page(entries, numEntries, offset, dformat, selected);
                print_statusbar(timeout, buffer[0] ? buffer : NULL, entries[offset].num + 1, entries[offset].num + lines - 1, etotal);
                winch = 0;
            }
        }
    }

    erase_display(2);
    color_positive();
    cursor_pos(1, 1);
    cursor_show();
    normal_screen();

    if (!ret) {
        ret = !format_entry(entries + selected, rformat, 1);
    }

    for (j = 0; j < numEntries; j++) {
        free(entries[j].key);
    }
    free(entries);
    return ret;
}
