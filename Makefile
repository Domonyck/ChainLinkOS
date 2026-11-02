CC      = gcc
CFLAGS  = -Wall -Iinclude
SRC     = "src"

all: server client

server: server/server.c $(SRC)/session.c $(SRC)/game.c
	$(CC) $(CFLAGS) -o $@ $^

client: server/client.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f server client
