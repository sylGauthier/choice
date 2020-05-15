CFLAGS ?= -std=c89 -pedantic -Wall -O2 -march=native -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500
PREFIX ?= /usr

.PHONY: clean install
choice: choice.o term.o
clean:
	rm -f $(wildcard choice *.o)
D=$(if $(DESTDIR),$(DESTDIR)/)$(PREFIX)
B=$(D)/$(or $(BINDIR),bin)
M=$(D)/$(or $(MANDIR),share/man)
install: choice
	mkdir -p $(B) $(M)/man1
	cp $< $(B)/
	cp $<.1 $(M)/man1
