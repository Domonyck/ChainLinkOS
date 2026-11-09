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
#include <signal.h>     /* signal, SIGINT, SIGTERM */
#include <time.h>       /* time */
#include <string.h>
#include <termios.h>    /* tcgetattr, tcsetattr: raw keyboard mode */
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ======== KEYBOARD: our own line editor ========
 *
 * In the terminal's normal ("cooked") mode the terminal itself edits the line
 * and some keys leak through as visible junk: Ctrl+R shows ^R, forward-Delete
 * shows ^[[3~, and so on. So while playing, the client reads single keys and
 * edits the line itself:
 *
 *     printable key        added to the line and shown
 *     Backspace / Delete   erases the last character (also the forward-Delete code ESC [ 3 ~)
 *     Enter                sends the line to the server
 *     arrows, Ctrl+R ...   ignored (nothing shown, nothing sent)
 *     Ctrl+D (empty line)  quits;  Ctrl+C quits
 *
 * The terminal is put back the way it was when the program ends.
 */

static struct termios saved_term;
static int term_saved = 0;

static void restore_terminal(void) {
    if (term_saved) tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
}

static void on_signal(int sig) {
    (void) sig;
    restore_terminal();
    _exit(1);
}

/* Switch the keyboard to single-key mode. Returns 1 on success, 0 if stdin is not a terminal. */
static int enable_raw_mode(void) {
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &saved_term) != 0) return 0;
    term_saved = 1;
    atexit(restore_terminal);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    struct termios raw = saved_term;
    raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);   /* no line editing, no automatic echo, no ^R/^V/^O handling */
    raw.c_cc[VMIN] = 1;                         /* wait for at least one key */
    raw.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
}

typedef struct {
    char text[256];   /* the line typed so far */
    size_t len;
    int state;        /* 0 normal, 1 after ESC, 2 inside ESC [ ..., 3 after ESC O */
    char csi[8];      /* the parameters of the ESC [ ... sequence being read */
    size_t csi_len;
} LineEditor;

/* Erase the last character (a whole UTF-8 character) and wipe it from the screen. */
static void erase_last(LineEditor *e) {
    if (e->len == 0) return;
    while (e->len > 0 && (e->text[e->len - 1] & 0xC0) == 0x80) e->len--;   /* UTF-8 continuation bytes */
    if (e->len > 0) e->len--;
    (void) !write(STDOUT_FILENO, "\b \b", 3);
}

/*
 * Feed one key byte to the editor.
 * Returns 1 when a full line is ready in e->text (Enter pressed), 2 to quit, 0 otherwise.
 */
static int editor_feed(LineEditor *e, unsigned char c) {
    if (e->state == 1) {                              /* just after ESC */
        e->state = (c == '[') ? 2 : (c == 'O') ? 3 : 0;
        e->csi_len = 0;
        return 0;
    }
    if (e->state == 3) { e->state = 0; return 0; }    /* ESC O x: skip x */
    if (e->state == 2) {                              /* ESC [ params final */
        if (c >= 0x40 && c <= 0x7E) {                 /* final byte ends the sequence */
            if (c == '~' && e->csi_len == 1 && e->csi[0] == '3') erase_last(e);   /* forward-Delete key */
            e->state = 0;
        } else if (e->csi_len < sizeof e->csi) {
            e->csi[e->csi_len++] = (char) c;
        }
        return 0;
    }

    if (c == 0x1B) { e->state = 1; return 0; }
    if (c == 0x7F || c == 0x08) { erase_last(e); return 0; }   /* Backspace / Delete */
    if (c == '\r' || c == '\n') return 1;                      /* Enter */
    if (c == 0x04) return (e->len == 0) ? 2 : 0;               /* Ctrl+D on an empty line */
    if (c < 0x20) return 0;                                    /* other control keys (Ctrl+R ...): ignored */

    if (e->len + 2 < sizeof e->text) {                         /* printable: add and show */
        e->text[e->len++] = (char) c;
        (void) !write(STDOUT_FILENO, &c, 1);
    }
    return 0;
}

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
	system("clear");
	printf("\033[0;97m  ██▀▀▀▀▀\033[0;37m \033[0;97m  ██   ██\033[0;37m  \033[0;97m  ██▀▀▀██\033[0;37m  \033[0;97m  ▀██▀\033[0;37m  \033[0;97m  ██▀▀▀▄▄\033[0;37m  \033[0;97m  ██      \033[0;97m  ▀██▀\033[0;37m  \033[0;97m  ██▀▀▀▄▄\033[0;37m  \033[0;97m  ██ \033[0;37m \033[0;97m ██\033[0;37m  \033[0m\n");
	printf("\033[0;37m█\033[0;97m ██\033[0;37m ██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n");
	printf("\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m█\033[0m\n");
	printf("\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██▀▀▀██ \033[0;37m██\033[0;97m ███████ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██▀▀▀▄▄ \033[0;37m█\033[0m\n");
	printf("\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m▄\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n");
	printf("\033[0;37m█\033[0;97m ██\033[0;37m ██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n");
	printf("\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█ \033[0;97m██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█ \033[0;97m██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n");
	printf("\033[0;37m█\033[0;97m ██\033[0;37m ██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n");
	printf("\033[0;37m▀\033[0;97m ██▄▄▄▄▄\033[0;37m ▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m▀▀\033[0;97m ▄██▄ \033[0;37m▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██\033[0;37m ▀▀\033[0;97m ██▄▄▄▄ \033[0;37m▀▀\033[0;97m ▄██▄ \033[0;37m▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██\033[0;37m ▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m▀\033[0m\n");
    printf("Connected!\n");

    /* 4. Talk: show what the server sends, send what the player types. */
    char buffer[256];
    int raw_mode = enable_raw_mode();   /* 0 when input is piped: then plain fgets is used */
    LineEditor editor;
    memset(&editor, 0, sizeof editor);

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

            /* A cleared screen means a new turn: whatever was half typed is gone from view, so drop it too. */
            buffer[n] = '\0';
            if (strstr(buffer, "\033[2J") != NULL) editor.len = 0;
        }

        /* Player typed something. */
        if (FD_ISSET(STDIN_FILENO, &readable)) {
            if (!raw_mode) {
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;   /* Ctrl+D */
                if (send(sock, buffer, strlen(buffer), 0) < 0) {
                    perror("send() failed");
                    break;
                }
                continue;
            }

            unsigned char keys[64];
            ssize_t got = read(STDIN_FILENO, keys, sizeof keys);
            if (got <= 0) break;

            int quit = 0;
            for (ssize_t i = 0; i < got && !quit; i++) {
                int action = editor_feed(&editor, keys[i]);
                if (action == 2) {
                    quit = 1;
                } else if (action == 1) {
                    editor.text[editor.len++] = '\n';
                    (void) !write(STDOUT_FILENO, "\r\n", 2);
                    if (send(sock, editor.text, editor.len, 0) < 0) {
                        perror("send() failed");
                        quit = 1;
                    }
                    editor.len = 0;
                }
            }
            if (quit) break;
        }
    }

    close(sock);
    return 0;
}