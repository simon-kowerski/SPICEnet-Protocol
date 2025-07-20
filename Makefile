CC = gcc
CFLAGS = -g -Wall -std=c99 -fsanitize=address,undefined -Iinclude 
CLINKS = -lm -Llib/spicenet -lspicenet

all: clean testing

testing: testing.c
	$(CC) $(CFLAGS) $^ $(CLINKS) -o $@ 

less: all
	rm *.o

clean: 
	rm -rf *.o testing
