# Server
#---------------------------------------------------------------------------
CC = gcc
OBJS =  LinkedList.o Mempool.o thpool.o pkt_Queue.o UDP_API.o SqlWrapper.o
CFLAGS = -std=gnu99 -lrt -lpthread -lzlog -lsqlite3
LIB = -L /usr/local/lib
#---------------------------------------------------------------------------
all: Server.out
Server.out: $(OBJS) BeDIS.h BeDIS.c Server.h Server.c
	$(CC) $(CFLAGS) $(OBJS) $(LIB) BeDIS.c Server.c -o ../bin/Server.out
LinkedList.o: LinkedList.c LinkedList.h
	$(CC) $(CFLAGS) LinkedList.c -c
thpool.o: thpool.c thpool.h
	$(CC) $(CFLAGS) thpool.c -c
Mempool.o: Mempool.c Mempool.h
	$(CC) $(CFLAGS) Mempool.c -c
UDP_API.o: UDP_API.c UDP_API.h pkt_Queue.o
	$(CC) $(CFLAGS) UDP_API.c -c
pkt_Queue.o: pkt_Queue.c pkt_Queue.h
	$(CC) $(CFLAGS) pkt_Queue.c -c
SqlWrapper.o: SqlWrapper.c SqlWrapper.h
	$(CC) $(CFLAGS) SqlWrapper.c -c
clean:
	rm -f *.o *.out *.h.gch
