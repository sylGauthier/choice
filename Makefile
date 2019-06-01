.PHONY: clean
CFLAGS ?= -std=c89 -pedantic -Wall -O2 -march=native -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500
choice: choice.o term.o
clean:
	rm -f $(wildcard choice *.o)
