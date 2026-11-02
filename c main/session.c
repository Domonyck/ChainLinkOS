/**
 * @file session.c
 * @brief Implementation of the Session module declared in session.h.
 *
 * Layout of this file:
 *   1. The Session struct (its fields are hidden from every other file)
 *   2. create_session / destroy_session
 *   3. run_session (placeholder for now)
 *
 * Why is the struct here and not in session.h? Keeping it out of the header
 * makes Session opaque: main.c can hold a Session * but cannot reach inside,
 * so no outside code can change s->mode or s->fds by accident.
 */

#include "session.h"

#include <stdlib.h>   /* calloc, free */
#include <stdio.h>    /* printf */

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
    /* free(NULL) does nothing, so no NULL check is needed. */
    free(s);
}

/* ======== RUNNING ======== */

void run_session(Session *s) {
    if (s == NULL) return;

    /* placeholder: proves the session stored the sockets correctly */
    printf("fd0: %d\n", s->fds[0]);
    printf("fd1: %d\n", s->fds[1]);
}