.PHONY: clean
CFLAGS ?= -std=c89 -pedantic -Wall -O2 -march=native
choice: choice.o term.o
clean:
	rm -f $(wildcard choice *.o)
