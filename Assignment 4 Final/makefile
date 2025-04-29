CC = gcc
CFLAGS = -pthread -lrt
AR = ar
ARFLAGS = rcs

all: initksocket sender receiver

libksocket.a: ksocket.o
	$(AR) $(ARFLAGS) libksocket.a ksocket.o

ksocket.o: ksocket.c
	$(CC) $(CFLAGS) -c ksocket.c -o ksocket.o

initksocket: initksocket.c libksocket.a
	$(CC) $(CFLAGS) -o initksocket initksocket.c -L. -lksocket

sender: user1.c libksocket.a
	$(CC) $(CFLAGS) -o sender user1.c -L. -lksocket

receiver: user2.c libksocket.a
	$(CC) $(CFLAGS) -o receiver user2.c -L. -lksocket

clean:
	rm -f initksocket sender receiver libksocket.a ksocket.o output*.txt