CC = gcc
CFLAGS = -g -Wall -std=c99 -fsanitize=address,undefined -I../../include
CLINKS = -lm

all: libspicenet.a

libspicenet.a: spp.o sndlp.o sntp_structures.o sntp.o snp.o
	ar rcs libspicenet.a $^

less: all
	rm *.o

clean: 
	rm -rf *.o libspicenet.a
