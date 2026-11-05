/**
 * @file client.c
 * @brief Player client: connects to the server by IP address and port.
 *
 * Prints everything the server sends and sends every line the player types.
 * select() watches the keyboard and the socket at the same time, so the
 * server can send updates at any moment (not only after you type).
 *
 * Compile: gcc -o client client.c
 * Run:     ./client 192.168.1.10 5000
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>     /* signal, SIGPIPE */
#include <time.h>       /* time */
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s server_ip port_no\n", argv[0]);
        return 1;
    }

    /* 1. Create a TCP socket. */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket() failed");
        return 1;
    }

    /* 2. Fill in the server's address from the IP string and port. */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
        close(sock);
        return 1;
    }

    /* 3. Connect. */
    printf("Connecting to %s:%s ...\n", argv[1], argv[2]);
    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sock);
        return 1;
    }
    printf("Connected!\n");

    /* 4. Talk: show what the server sends, send what the player types. */
    char buffer[256];
    for (;;) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(STDIN_FILENO, &readable);
        FD_SET(sock, &readable);

        if (select(sock + 1, &readable, NULL, NULL, NULL) < 0) {
            perror("select() failed");
            break;
        }

        /* Server sent something. */
        if (FD_ISSET(sock, &readable)) {
            int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) {   /* 0 = server closed, <0 = error */
                printf("Disconnected from server.\n");
                break;
            }
            fwrite(buffer, 1, n, stdout);
            fflush(stdout);
        }

        /* Player typed a line. */
        if (FD_ISSET(STDIN_FILENO, &readable)) {
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;   /* Ctrl+D */
            if (send(sock, buffer, strlen(buffer), 0) < 0) {
                perror("send() failed");
                break;
            }
        }
    }

    close(sock);
    return 0;
}
