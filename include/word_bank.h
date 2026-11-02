/**
 * @file word_bank.h
 * @brief Public interface of the WordBank module.
 *
 * A WordBank loads the word chains stored in data/classicmode.json or
 * data/blitzmode.json and hands them out to the rest of the game. Session
 * uses it to pick a chain when the game master supplies the list.
 *
 * Expected JSON shape (both keys are optional, but at least one must have
 * content):
 *
 *   {
 *     "chains":    [ ["fire", "work", "shop"], ["sun", "light", "house"] ],
 *     "word_bank": [ "extra", "single", "words" ]
 *   }
 *
 * WordBank is an OPAQUE type: this header only declares it, and the fields
 * are defined in word_bank.c.
 *
 * Ownership rules (read these first):
 *   - wb_load() allocates a WordBank. The CALLER owns it and must call
 *     wb_free() exactly once.
 *   - Every const char * / const char *const * returned by the wb_get_*
 *     functions points INTO the WordBank. Do not free it, and do not use it
 *     after wb_free(). game_init() copies the words, so passing a chain
 *     straight to it is safe.
 *
 * Randomness: wb_get_random_* use rand(). Call srand() once at program start
 * (e.g. in server.c) or every run will pick the same chain.
 */

#ifndef WORD_BANK_H
#define WORD_BANK_H

#include <stddef.h>     // size_t
#include <stdbool.h>    // bool (used by word_bank.c)

/**
 * @brief The loaded contents of one JSON data file (opaque; see word_bank.c).
 */
typedef struct WordBank WordBank;

/* ========== LOAD / FREE ========== */

/**
 * @brief Load chains (and the optional flat word list) from a JSON file.
 *
 * @param path  Path to the file, e.g. "data/classicmode.json". Relative paths
 *              are resolved from the directory the program is run in.
 * @return A new WordBank, or NULL if path is NULL, the file cannot be read,
 *         the JSON is malformed, or the file has neither "chains" nor
 *         "word_bank" content.
 *
 * Ownership: the caller must pass the result to wb_free().
 */
WordBank *wb_load(const char *path);

/**
 * @brief Release a WordBank created by wb_load().
 *
 * @param wb  The word bank. Safe to call with NULL.
 */
void wb_free(WordBank *wb);

/* ========== CHAINS ========== */

/**
 * @brief Number of chains loaded from the "chains" key.
 *
 * @return The count, or 0 if wb is NULL.
 */
size_t wb_chain_count(const WordBank *wb);

/**
 * @brief Get chain number idx.
 *
 * @param wb   The word bank.
 * @param idx  Zero-based index, must be below wb_chain_count().
 * @param n    Out: number of words in the chain. Set to 0 on failure.
 *             May be NULL if you do not need the length.
 * @return The words of the chain, or NULL if wb is NULL or idx is out of
 *         range. The result plugs straight into game_init(g, words, n, ...).
 *
 * The memory belongs to the WordBank (see the ownership rules above).
 */
const char *const *wb_get_chain(const WordBank *wb, size_t idx, size_t *n);

/**
 * @brief Get a randomly chosen chain.
 *
 * @param wb  The word bank.
 * @param n   Out: number of words in the chain. Set to 0 on failure.
 *            May be NULL.
 * @return The words of a random chain, or NULL if wb is NULL or has no
 *         chains.
 *
 * Remember to call srand() once before using this.
 */
const char *const *wb_get_random_chain(const WordBank *wb, size_t *n);

/* ========== FLAT WORD BANK ========== */

/**
 * @brief Number of words loaded from the "word_bank" key.
 *
 * @return The count, or 0 if wb is NULL or the key is missing or empty.
 */
size_t wb_bank_count(const WordBank *wb);

/**
 * @brief Get word number idx from the flat word list.
 *
 * @return The word, or NULL if wb is NULL or idx is out of range.
 */
const char *wb_get_bank_word(const WordBank *wb, size_t idx);

/**
 * @brief Get a randomly chosen word from the flat word list.
 *
 * @return The word, or NULL if wb is NULL or the list is empty.
 *
 * Remember to call srand() once before using this.
 */
const char *wb_get_random_bank_word(const WordBank *wb);

#endif /* WORD_BANK_H */
