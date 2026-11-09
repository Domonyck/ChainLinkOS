/**
 * @file server.c
 * @brief Entry point of the server (replaces main.c).
 *
 * Opens a TCP port, waits for two players to connect, hands both sockets to
 * a Session (player 0 = first to connect, player 1 = second), runs the match,
 * then cleans up.
 *
 * Without running make -j, do this:
 * Compile: make            (builds server_app)
 * Run:     ./server_app 5000     (from the project root, so data/ is found)
 */

#include "../include/session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>     /* signal, SIGPIPE */
#include <time.h>       /* time */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Wait for one player to connect and return its socket (or -1 on error). */
static int accept_player(int server_sock, int player_no) {
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);

    printf("Waiting for player %d ...\n", player_no);
    int fd = accept(server_sock, (struct sockaddr *) &client_addr, &client_size);
    if (fd < 0) {
        perror("accept() failed");
        return -1;
    }

    printf("Player %d connected from %s\n", player_no, inet_ntoa(client_addr.sin_addr));
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s port_no\n", argv[0]);
        return 1;
    }
    int port_no = atoi(argv[1]);

    /* A dropped player must not kill the server: ignoring SIGPIPE turns a
       send() to a closed socket into a normal error the session can handle. */
    signal(SIGPIPE, SIG_IGN);

    /* Seed the random number generator once, so the coin flip and the random
       chain pick are different every run. */
    srand((unsigned) time(NULL));

    /* 1. Create a TCP socket. */
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket() failed");
        return 1;
    }

    /* Lets you restart the server right away without "Address already in use". */
    int yes = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    /* 2. Bind it to the port on every network interface (any IP of this computer). */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_no);

    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(server_sock);
        return 1;
    }

    /* 3. Listen. Only two players are ever needed. */
    listen(server_sock, 2);
    printf("Server listening on port %d ...\n", port_no);

    /* 4. Accept exactly two players. */
    int fd0 = accept_player(server_sock, 0);
    if (fd0 < 0) { close(server_sock); return 1; }

    const char *wait_msg = "Connected! Waiting for the other player...\n";
    send(fd0, wait_msg, strlen(wait_msg), 0);

    int fd1 = accept_player(server_sock, 1);
    if (fd1 < 0) { close(fd0); close(server_sock); return 1; }

    /* 5. Hand both sockets to the game session. */
    Session *s = create_session(fd0, fd1);
    if (s == NULL) {
        fprintf(stderr, "Session was not created.\n");
        close(fd0);
        close(fd1);
        close(server_sock);
        return 1;
    }

    run_session(s);
    destroy_session(s);

    /* The session never closes the sockets, so main does it. */
    close(fd0);
    close(fd1);
    close(server_sock);
    return 0;
}