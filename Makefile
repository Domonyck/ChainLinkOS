# run server: gcc -std=c99 -Wall -Wextra -Iinclude -o server_app server/server.c src/session.c src/game.c core/ascii_art.c core/ascii_art_announce.c core/blitz_mode.c core/classic_mode.c core/word_bank.c
# run client: gcc -std=c99 -Wall -Wextra -Iinclude -o client_app server/client.c
# ============================================================================

# --- Settings ---------------------------------------------------------------
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Iinclude
SAN     = -fsanitize=address,undefined

SRC     = src
CORE    = core

# Every .c file the server needs to link into one program
SERVER_SRC = server/server.c $(SRC)/session.c $(SRC)/game.c $(CORE)/ascii_art.c \
             $(CORE)/ascii_art_announce.c $(CORE)/blitz_mode.c $(CORE)/classic_mode.c $(CORE)/word_bank.c

all: server_app client_app

server_app: $(SERVER_SRC) $(wildcard include/*.h)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC)

client_app: server/client.c
	$(CC) $(CFLAGS) -o $@ $^

test_blitz: $(CORE)/blitz_mode.c $(SRC)/game.c tests/test_blitz_mode.c
	$(CC) $(CFLAGS) $(SAN) -o $@ $^
	./$@

clean:
	rm -f server_app client_app test_blitz

.PHONY: all clean
