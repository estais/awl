#
# Makefile for awl
#

CC = gcc
CFLAGS = -O2 -Wall -Wextra -pedantic -g -Isrc
LDLIBS = -lm

BINDIR = bin
TARGET = $(BINDIR)/awl

OBJS = \
       src/err.o \
       src/mem.o \
       src/vec.o \
       src/file.o \
       src/strbuf.o \
       src/lexer.o \
       src/main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	if ! [ -d $(BINDIR) ]; then mkdir $(BINDIR); fi
	$(CC) $^ $(LDLIBS) -o $@

.c.o:
	$(CC) $< $(CFLAGS) -c -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	if [ -d $(BINDIR) ]; then rm -rf $(BINDIR); fi
