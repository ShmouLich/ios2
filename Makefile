CFLAGS= -std=gnu99 -Wall -Wextra -Werror -pedantic -pthread
CC=gcc

proj2 : proj2.c
	$(CC) $(CFLAGS) -o proj2 proj2.c

pack:
	zip proj2.zip *.c Makefile

clean :
	rm -f *.o proj2