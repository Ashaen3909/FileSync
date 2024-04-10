CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

SRCS = mysync.c fileutil.c
OBJS = $(SRCS:.c=.o)

all: mysync

mysync: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o mysync

mysync.o: mysync.c mysync.h fileutil.h
	$(CC) $(CFLAGS) -c mysync.c

fileutil.o: fileutil.c fileutil.h
	$(CC) $(CFLAGS) -c fileutil.c

clean:
	rm -f $(OBJS) mysync


