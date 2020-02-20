CC = gcc
CFLAGS = -shared -fPIC -O2 -Wall -I ..
BASICFLAGS = -O2 -w -I .

all: adder fib

base: adder1

adder: adder.c csapp.o
	$(CC) $(CFLAGS) -o adder.so adder.c csapp.o

fib: fib.c csapp.o
	$(CC) $(CFLAGS) -o fib.so fib.c csapp.o

adder1: adder1.c
	$(CC) $(BASICFLAGS) -o adder1 adder1.c

clean:
	rm -f *.o adder fib *~
