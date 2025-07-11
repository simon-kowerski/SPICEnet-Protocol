CC = gcc
CFLAGS = -g -Wall -std=c99 -fsanitize=address,undefined -Iinclude 
CLINKS = -lm -Llibrary/spicenet -lspicenet

all: testing

testing: testing.c
	$(CC) $(CFLAGS) $^ $(CLINKS) -o $@ 

less: all
	rm *.o

clean: 
	rm -rf *.o testing
