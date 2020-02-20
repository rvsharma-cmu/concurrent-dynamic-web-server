CC = gcc
CFLAGS = -shared -fPIC -O2 -Wall -I ..

all: adder fib

adder: adder.c csapp.o
	$(CC) $(CFLAGS) -o adder.so adder.c csapp.o

fib: fib.c csapp.o
	$(CC) $(CFLAGS) -o fib.so fib.c csapp.o

clean:
	rm -f *.o adder fib *~
