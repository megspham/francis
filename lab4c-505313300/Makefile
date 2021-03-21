# NAME: Megan Pham
# EMAIL: megspham@ucla.edu
# ID: 505313300

OPTIMIZE = -O2
CC = gcc
CFLAGS = $(OPTIMIZE) -g3 -Wall -Wextra

default: lab4c_tcp.c lab4c_tls.c
	$(CC) $(CFLAGS) lab4c_tcp.c -o lab4c_tcp -lmraa -lm
	$(CC) $(CFLAGS) lab4c_tls.c -o lab4c_tls -lmraa -lm -lssl -lcrypto

dist: 
	tar -czf lab4c-505313300.tar.gz lab4c_tcp.c lab4c_tls.c README Makefile

clean: 
	rm -f lab4c *.tar.gz 
