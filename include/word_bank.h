#ifndef WORD_BANK_H
#define WORD_BANK_H

#include <stddef.h>    
#include <stdbool.h>    

typedef struct WordBank WordBank;


WordBank *wb_load(const char *path);

void wb_free(WordBank *wb);

size_t wb_chain_count(const WordBank *wb);

const char *const *wb_get_chain(const WordBank *wb, size_t idx, size_t *n);

const char *const *wb_get_random_chain(const WordBank *wb, size_t *n);

size_t wb_bank_count(const WordBank *wb);

const char *wb_get_bank_word(const WordBank *wb, size_t idx);

const char *wb_get_random_bank_word(const WordBank *wb);

#endif /* WORD_BANK_H */
