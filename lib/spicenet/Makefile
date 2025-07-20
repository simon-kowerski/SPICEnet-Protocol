CC = gcc
CFLAGS = -g -Wall -std=c99 -fsanitize=address,undefined -I../../include
CLINKS = -lm

all: libspicenet.a

libspicenet.a: spp.o sndlp.o sntp_structures.o sntp-1.o snp.o cop1.o
	ar rcs libspicenet.a $^

cop1: cop1.o
	$(CC) $(CFLAGS) cop1.o -o cop1

cop1.o: cop1.c
	$(CC) $(CFLAGS) -c cop1.c

less: all
	rm *.o

clean: 
	rm -rf *.o libspicenet.a
