CC = gcc
CFLAGS = -g -Wall -std=c99 -fsanitize=address,undefined -Iinclude
CLINKS = -lm

all: obctest

obctest: obctest.o sndlp.o spp.o include/spicenet/*.h
	$(CC) $(CFLAGS) $(CLINKS) -o $@ $^


clienttest: clienttest.o sndlp.o spp.o include/spicenet/*.h
	$(CC) $(CFLAGS) $(CLINKS) -o $@ $^

less: all
	rm *.o

clean: 
	rm -rf *.o obctest
