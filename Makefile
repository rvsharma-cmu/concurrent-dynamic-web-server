CC = gcc
CFLAGS =-g -O2 -Wall
# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LDLIBS=-lpthread

all: webserver cgi

webserver: webserver.c csapp.c

cgi:
	(cd cgi-bin; make)

tar:
	(cd ..; tar cvf webserver.tar webserver)

clean:
	rm -f *.o tiny *~
	(cd cgi-bin; make clean)

