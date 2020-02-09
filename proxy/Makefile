#
# Makefile for Proxy Lab
#
# You may modify this file any way you like (except for the handin
# rule). Autolab will execute the command "make" on your specific
# Makefile to build your proxy from sources.
#
CC = gcc
CFLAGS = -g -Og -Wall -std=c99
LDFLAGS = -lpthread
TERM = f18

proxy: proxy.o csapp.o cache.o

all: proxy tiny-code

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

cache.o: cache.c cache.h
	$(CC) $(CFLAGS) -c cache.c

tiny-code:
	(cd tiny; make)

# Creates a tarball in ../proxylab-handin.tar that you should then
# hand in to Autolab. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf proxylab-handin.tar proxylab-$(TERM)-*/*.{c,h} proxylab-$(TERM)-*/Makefile)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz
	rm -rf logs source_files response_files results.log
	(cd tiny; make clean)

