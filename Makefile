# all: serwer

# serwer: serwer.o err.o utilities.o
	# cc -Wall -pthread -o serwer serwer.o err.o utilities.o

# serwer.o: serwer.c err.h utilities.h
	# cc -Wall -c serwer.c

# err.o: err.c err.h
	# cc -Wall -c err.c

clean:
	rm -f *.o serwer klient
