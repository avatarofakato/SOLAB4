all: simple_test

# TODO why we need -O3?
simple_test: libpagesim.a simple_test.o err.o
	cc -Wall -pthread simple_test.c libpagesim.a err.o

libpagesim.a: pagesim.o
	ar rcs libpagesim.a pagesim.o

pagesim.o: pagesim.c
	cc -Wall -c pagesim.c

err.o: err.c err.h
	cc -Wall -c err.c

# TODO delete *.out
clean:
	rm -f *.o *.a *.out simple_test
