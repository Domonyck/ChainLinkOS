/**
 * @file session.h
 * @brief Public interface of the Session module (the "game master").
 *
 * A Session hosts one match between two connected players: it asks the setup
 * questions, flips the coin, relays messages between the sockets and the mode
 * (the referee), and announces the result.
 *
 * This module never decides whether a guess is correct (that is game.h) and
 * never decides what a wrong guess costs (that is the mode). It only
 * coordinates.
 *
 * Session is an OPAQUE type: this header only declares it, and the fields are
 * defined in session.c. Code outside session.c can hold a Session * but cannot
 * read or change what is inside it.
 *
 * Ownership rules (read these first):
 *   - create_session() allocates a Session. The CALLER owns it.
 *   - Call destroy_session() exactly once when the match is over.
 *   - The Session does NOT own the sockets: it stores their numbers but never
 *     closes them. Whoever accepted the connections closes them.
 *
 * Current status: run_session() asks player 0 for the mode, then plays a full
 * Blitz match (coin flip, turns, clocks, results). Classic is not wired yet.
 */

#ifndef SESSION_H
#define SESSION_H

/**
 * @brief Which ruleset the match uses.
 *
 * Values are prefixed (MODE_) because C enums share one global namespace.
 */
typedef enum {
    MODE_CLASSIC,    /**< Turn ends on a wrong guess; players may supply the list. */
    MODE_BLITZ       /**< 30-second clock per player; the game master supplies the list. */
} GameMode;

/**
 * @brief Who provides the word list. Only asked in Classic mode.
 */
typedef enum {
    LIST_GAME_MASTER,    /**< The server picks a chain from the files in data/. */
    LIST_PLAYER          /**< A player writes the list; it must be validated. */
} ListSource;

/**
 * @brief One hosted match between two players (opaque; see session.c).
 */
typedef struct Session Session;

/**
 * @brief Create a session for two already-connected players.
 *
 * @param fd0  Socket of player 0.
 * @param fd1  Socket of player 1.
 * @return A new Session, or NULL if memory could not be allocated.
 *
 * The fields start zeroed, so the mode defaults to MODE_CLASSIC and the list
 * source to LIST_GAME_MASTER until setup changes them.
 *
 * Ownership: the caller must pass the result to destroy_session().
 */
Session *create_session(int fd0, int fd1);

/**
 * @brief Run the whole match from start to finish, then return.
 *
 * @param s  The session. Does nothing if NULL.
 *
 * Asks player 0 to choose the mode, loads the word chain, and for Blitz runs
 * the whole match until it ends (a finished list, both clocks out, or a
 * disconnect). Classic prints "not available yet" for now.
 */
void run_session(Session *s);

/**
 * @brief Release a session created by create_session().
 *
 * @param s  The session. Safe to call with NULL.
 *
 * Does not close the sockets (see the ownership rules above).
 */
void destroy_session(Session *s);

#endif /* SESSION_H */