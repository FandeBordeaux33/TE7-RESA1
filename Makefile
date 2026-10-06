CFLAGS=-Wall

all: client server

server: server.o client_list.o common.o
client: client.o common.o

clean:
	rm -f client server *.o

.PHONY: all clean
