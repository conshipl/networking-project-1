HOSTNAME := $(shell hostname)

client: client.o
	g++ -g client.o -o client

client.o: client.c
	g++ -g -c client.c

server: server.o
	g++ -g server.o -o server

server.o: server.c
	g++ -g -c server.c

clean:
	rm -f *.o client server

c: client
	./client $(HOSTNAME)

s: server
	./server $(HOSTNAME)