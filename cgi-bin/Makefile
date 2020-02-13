CC = gcc
CFLAGS = -shared -fPIC -O2 -Wall -I ..

all: adder

adder: adder.c csapp.o
	$(CC) $(CFLAGS) -o adder.so adder.c csapp.o

clean:
	rm -f *.o adder *~
