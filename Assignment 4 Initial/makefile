CC = gcc
CFLAGS = -Wall -g 
LIBNAME = libksocket.a
LIBS = -L. -lksocket 

all: $(LIBNAME) initksocket user1 user2

apps: user1 user2

$(LIBNAME): ksocket.o
	ar rcs $(LIBNAME) ksocket.o

ksocket.o: ksocket.c ksocket.h
	$(CC) $(CFLAGS) -c ksocket.c

initksocket: initksocket.o $(LIBNAME)
	$(CC) $(CFLAGS) -o initksocket initksocket.o $(LIBS)

initksocket.o: initksocket.c ksocket.h
	$(CC) $(CFLAGS) -c initksocket.c

user1: user1.o $(LIBNAME)
	$(CC) $(CFLAGS) -o user1 user1.o $(LIBS)

user1.o: user1.c ksocket.h
	$(CC) $(CFLAGS) -c user1.c

user2: user2.o $(LIBNAME)
	$(CC) $(CFLAGS) -o user2 user2.o $(LIBS)

user2.o: user2.c ksocket.h
	$(CC) $(CFLAGS) -c user2.c

server: initksocket
	./initksocket
debug_server: initksocket
	valgrind ./initksocket

sender: user1
	./user1 127.0.0.1 8000 127.0.0.1 8001
debug_sender: user1
	valgrind ./user1 127.0.0.1 8000 127.0.0.1 8001

receiver: user2
	./user2 127.0.0.1 8001 127.0.0.1 8000
debug_receiver: user2
	valgrind ./user2 127.0.0.1 8001 127.0.0.1 8000

clean:
	rm -f *.o *.a initksocket user1 user2
	rm -f output_*.txt