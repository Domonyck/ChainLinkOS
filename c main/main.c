/**
 * @file main.c
 * @brief Entry point of the server.
 *
 * Eventually: accept two clients, build a Session around their sockets, run
 * the match, then clean up. For now the sockets do not exist yet, so it uses
 * -1 as a placeholder for both and just proves create/run/destroy work.
 *
 * Why so little here? main only wires things together. All the game logic
 * lives in the modules it calls.
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