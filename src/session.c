/*
It runs one match from start to finish, on the server.

It never decides whether a guess is correct (that’s game.c) or what a wrong guess costs (that’s the mode).
Right now it only loads a chain and prints it, so most of its job is still to be built.
*/

#include "session.h"
#include "word_bank.h"

#include <stdlib.h>   /* calloc, free */
#include <stdio.h>    /* printf */

/* Data files, relative to the directory the server is started from (the repo root). */
#define CLASSIC_DATA_FILE "data/classicmode.json"
#define BLITZ_DATA_FILE   "data/blitzmode.json"

/* ======== THE STRUCT ======== */

/**
 * Everything one match needs to remember.
 *
 * More fields (the Game, the mode referee, the current phase) are added in
 * later steps, once those modules exist.
 */
struct Session {
    int fds[2];          /**< Sockets of player 0 and player 1. Not owned: never closed here. */
    GameMode mode;       /**< Classic or Blitz. */
    ListSource source;   /**< Who provides the list (only asked in Classic). */

    WordBank *bank;               /**< Loaded JSON data. Owned: freed in destroy_session. */
    const char *const *chain;     /**< The chosen chain. Points INTO bank, so never freed on its own. */
    size_t chain_len;             /**< Number of words in chain. */
};

/* ======== SETUP / TEARDOWN ======== */

Session *create_session(int fd0, int fd1) {
    /* calloc zeroes every byte, so mode and source start at their first enum value. */
    Session *s = calloc(1, sizeof *s);

    if (s == NULL) return s;   /* out of memory: the caller checks for NULL */

    /* Remember the sockets (destination = source: the session's field gets the argument). */
    s->fds[0] = fd0;
    s->fds[1] = fd1;

    return s;   /* without this line the caller receives garbage */
}

void destroy_session(Session *s) {
    if (s == NULL) return;

    /* The chain points into the bank, so freeing the bank invalidates it too. */
    wb_free(s->bank);   /* safe with NULL */
    free(s);
}

/* ======== WORD LIST ======== */

/**
 * Load the JSON file that matches the mode and pick a random chain from it.
 *
 * Blitz always uses the game master's list. Classic uses it unless a player
 * is supposed to write the list (not implemented yet).
 *
 * @return 0 on success, -1 on failure (a message is printed to stderr).
 */
static int load_chain(Session *s) {
    if (s->mode == MODE_CLASSIC && s->source == LIST_PLAYER) {
        fprintf(stderr, "Player-made lists are not implemented yet.\n");
        return -1;
    }

    const char *path = (s->mode == MODE_BLITZ) ? BLITZ_DATA_FILE : CLASSIC_DATA_FILE;

    /* Drop a previously loaded bank so calling this twice does not leak. */
    wb_free(s->bank);
    s->bank = wb_load(path);
    if (s->bank == NULL) {
        fprintf(stderr, "Could not load word list from %s (run the server from the repo root).\n", path);
        s->chain = NULL;
        s->chain_len = 0;
        return -1;
    }

    s->chain = wb_get_random_chain(s->bank, &s->chain_len);
    if (s->chain == NULL) {
        fprintf(stderr, "%s has no chains.\n", path);
        return -1;
    }

    return 0;
}

/* ======== RUNNING ======== */

void run_session(Session *s) {
    if (s == NULL) return;

    /* placeholder: proves the session stored the sockets correctly */
    printf("fd0: %d\n", s->fds[0]);
    printf("fd1: %d\n", s->fds[1]);

    /* Pick the word chain for this match. */
    if (load_chain(s) != 0) return;

    /* placeholder: proves the chain was read from the JSON file */
    printf("Chain (%zu words):", s->chain_len);
    for (size_t i = 0; i < s->chain_len; i++) printf(" %s", s->chain[i]);
    printf("\n");
}
