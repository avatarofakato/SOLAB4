all: simple_test

simple_test: libpagesim.a simple_test.o
	cc simple_test.cc libpagesim.a

libpagesim.a: pagesim.o
	ar rcs libpagesim.a pagesim.o

pagesim.o: pagesim.cc
	cc -Wall -c pagesim.cc

# err.o: err.c err.h
	# cc -Wall -c err.c

# TODO delete *.out
clean:
	rm -f *.o *.a *.out simple_test
