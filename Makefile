CC = gcc
CFLAGS = -g -O2 -w -rdynamic -I .
BASICFLAGS = -O2 -w -I .
# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread -ldl

all: tiny cgi

tiny: webserver.c csapp.o
	$(CC) $(CFLAGS) -o webserver webserver.c csapp.o $(LIB)

baseline: tiny_base.c csapp.o
	$(CC) $(BASICFLAGS) -o tiny_base tiny_base.c csapp.o $(LIB)

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

cgi:
	(cd cgi-bin; make)

clean:
	rm -f *.o webserver *~
	(cd cgi-bin; make clean)



