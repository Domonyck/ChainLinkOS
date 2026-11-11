# ============================================================================
# ChainLinkOS Makefile
#
# A Makefile is a list of recipes. Each recipe has this shape:
#
#     target: prerequisites
#     <TAB>command that builds the target
#
# "make target" runs the command only if a prerequisite is newer than the
# target, so unchanged programs are not rebuilt. Plain "make" builds the first
# target in the file (all).
#
# HOW TO USE (run from the ChainLinkOS root, the folder holding data/):
#
#     make                 build server_app and client_app
#     make test_blitz      build and run the Blitz tests (with sanitizers)
#     make clean           delete the built files
#
#     ./server_app 5000                 start the server (terminal 1)
#     ./client_app 127.0.0.1 5000       player 0 (terminal 2)
#     ./client_app 127.0.0.1 5000       player 1 (terminal 3)
#
# Run the server from this folder: session.c opens "data/*.json" relative to
# where the server is started.
#
# WHEN YOU ADD A FILE:
#     a new .c the server needs   -> add it to SERVER_SRC
#     a new .h                    -> nothing to do (include/*.h is picked up)
#     a new test                  -> copy the test_blitz recipe
#
# GOTCHAS:
#     - Recipe lines must start with a TAB, not spaces ("missing separator").
#     - Never name an output "server" or "client": those are folders.
#     - "undefined reference" means a .c file is missing from SERVER_SRC.
#     - Only ONE file may contain main() per program.
# ============================================================================

# --- Settings ---------------------------------------------------------------
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Iinclude
SAN     = -fsanitize=address,undefined

SRC     = src
CORE    = core

# Every .c file the server needs to link into one program
SERVER_SRC = server/server.c $(SRC)/session.c $(SRC)/game.c $(CORE)/ascii_art.c \
             $(CORE)/blitz_mode.c $(CORE)/classic_mode.c $(CORE)/word_bank.c

# --- Targets ----------------------------------------------------------------
# Default target: what plain "make" builds
all: server_app client_app

# The server. The headers are prerequisites so editing a .h triggers a rebuild.
#   $@ = the target name (server_app)
server_app: $(SERVER_SRC) $(wildcard include/*.h)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC)

# The client. $^ = all the prerequisites (server/client.c)
client_app: server/client.c
	$(CC) $(CFLAGS) -o $@ $^

# The Blitz tests: build with sanitizers, then run them right away.
# Expected output: all blitz mode tests passed
test_blitz: $(CORE)/blitz_mode.c $(SRC)/game.c tests/test_blitz_mode.c
	$(CC) $(CFLAGS) $(SAN) -o $@ $^
	./$@

# Delete everything make built
clean:
	rm -f server_app client_app test_blitz

# These are commands, not files, so make never mistakes them for up-to-date files
.PHONY: all clean
