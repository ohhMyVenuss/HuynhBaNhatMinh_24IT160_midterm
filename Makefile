#
# Makefile for Midterm Project: Implement ls(1)
# Supports both BSD make (NetBSD) and GNU make (Linux)
#

CC ?= gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -g -O2
LDFLAGS =

PROG = ls
SRCS = main.c util.c sort.c display.c traverse.c
OBJS = $(SRCS:.c=.o)
HEADERS = ls.h util.h sort.h display.h traverse.h

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(PROG) $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(PROG)
	@chmod +x test_suite.sh
	@./test_suite.sh

clean:
	rm -f $(PROG) $(OBJS) *.core

.PHONY: all clean test
