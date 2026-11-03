/*
It’s the original program entry point. 
It created a session with dummy sockets (-1, -1), ran it and destroyed it. It was a placeholder from when sockets didn’t exist yet.

server.c has replaced it: it does the same thing, but with real sockets that wait for two players to connect. 
A program can only have one main, so main.c should be removed from the build.
*/

#include "session.h"

#include <stdio.h>   /* fprintf */

int main(void) {
    /* -1, -1 are dummy socket numbers until real connections are accepted. */
    Session *s = create_session(-1, -1);

    /* create_session returns NULL when memory runs out: never use it unchecked. */
    if (s == NULL) {
        fprintf(stderr, "Session was not created.\n");
        return 1;   /* non-zero exit status tells the shell it failed */
    }

    run_session(s);

    destroy_session(s);   /* exactly once, after the match is over */

    return 0;
}