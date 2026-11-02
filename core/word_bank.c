#include "word_bank.h"

#include <stdio.h>      // fopen, fread, fclose
#include <stdlib.h>     // malloc, realloc, free, rand
#include <string.h>     // strlen, strstr, strchr, memcpy

/* ======== TYPES ======== */

// A growable list of heap strings
typedef struct {
    char  **items;
    size_t  count;
    size_t  cap;
} StrList;

struct WordBank {
    StrList *chains;        // every chain loaded from the "chains" key
    size_t   chain_count;
    size_t   chain_cap;
    StrList  bank;          // the flat list from the "word_bank" key (may be empty)
};

/* ======== INTERNAL HELPERS ======== */

static void list_free(StrList *l) {
    for (size_t i = 0; i < l->count; i++) free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->count = l->cap = 0;
}

static bool list_push(StrList *l, char *s) {
    if (l->count == l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 8;
        char **tmp = realloc(l->items, ncap * sizeof(char *));
        if (!tmp) return false;
        l->items = tmp;
        l->cap = ncap;
    }
    l->items[l->count++] = s;
    return true;
}

// read a whole file into a null-terminated heap buffer
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

// p points at an opening quote; returns the position after the closing quote
static const char *parse_string(const char *p, char **out) {
    if (*p != '"') return NULL;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return NULL;

    size_t n = (size_t)(end - p);
    char *s = malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, p, n);
    s[n] = '\0';

    *out = s;
    return end + 1;
}

// p points at '['; fills l with the strings and returns the position after ']'
static const char *parse_string_list(const char *p, StrList *l) {
    if (*p != '[') return NULL;
    p++;

    for (;;) {
        p = skip_ws(p);
        if (*p == ']') return p + 1;
        if (*p == ',') { p++; continue; }

        char *s = NULL;
        p = parse_string(p, &s);
        if (!p) return NULL;
        if (!list_push(l, s)) { free(s); return NULL; }
    }
}

// find the '[' that follows a key such as "chains"; NULL if the key is missing
static const char *find_array(const char *text, const char *key) {
    const char *p = strstr(text, key);
    if (!p) return NULL;
    p += strlen(key);
    p = skip_ws(p);
    if (*p != ':') return NULL;
    p = skip_ws(p + 1);
    return (*p == '[') ? p : NULL;
}

// parse the array of arrays under "chains"
static bool parse_chains(WordBank *wb, const char *p) {
    p++;    // skip the outer '['

    for (;;) {
        p = skip_ws(p);
        if (*p == ']') return true;
        if (*p == ',') { p++; continue; }
        if (*p != '[') return false;

        StrList chain = {0};
        p = parse_string_list(p, &chain);
        if (!p || chain.count == 0) { list_free(&chain); return false; }

        if (wb->chain_count == wb->chain_cap) {
            size_t ncap = wb->chain_cap ? wb->chain_cap * 2 : 4;
            StrList *tmp = realloc(wb->chains, ncap * sizeof(StrList));
            if (!tmp) { list_free(&chain); return false; }
            wb->chains = tmp;
            wb->chain_cap = ncap;
        }
        wb->chains[wb->chain_count++] = chain;
    }
}

/* ======== PUBLIC FUNCTIONS ======== */

// load chains (and the optional flat word bank) from a json file; NULL on any failure
WordBank *wb_load(const char *path) {
    if (!path) return NULL;

    char *text = read_file(path);
    if (!text) return NULL;

    WordBank *wb = calloc(1, sizeof *wb);
    if (!wb) { free(text); return NULL; }

    const char *chains = find_array(text, "\"chains\"");
    if (chains && !parse_chains(wb, chains)) goto fail;

    const char *bank = find_array(text, "\"word_bank\"");
    if (bank && !parse_string_list(bank, &wb->bank)) goto fail;

    // a file with neither key is useless
    if (wb->chain_count == 0 && wb->bank.count == 0) goto fail;

    free(text);
    return wb;

fail:
    free(text);
    wb_free(wb);
    return NULL;
}

void wb_free(WordBank *wb) {
    if (!wb) return;
    for (size_t i = 0; i < wb->chain_count; i++) list_free(&wb->chains[i]);
    free(wb->chains);
    list_free(&wb->bank);
    free(wb);
}

size_t wb_chain_count(const WordBank *wb) {
    return wb ? wb->chain_count : 0;
}

// get chain number idx; the result plugs straight into game_init (words, n)
const char *const *wb_get_chain(const WordBank *wb, size_t idx, size_t *n) {
    if (!wb || idx >= wb->chain_count) {
        if (n) *n = 0;
        return NULL;
    }
    if (n) *n = wb->chains[idx].count;
    return (const char *const *)wb->chains[idx].items;
}

// get a random chain (call srand once in main before using this)
const char *const *wb_get_random_chain(const WordBank *wb, size_t *n) {
    if (!wb || wb->chain_count == 0) {
        if (n) *n = 0;
        return NULL;
    }
    return wb_get_chain(wb, (size_t)rand() % wb->chain_count, n);
}

size_t wb_bank_count(const WordBank *wb) {
    return wb ? wb->bank.count : 0;
}

// get word number idx from the flat word bank; NULL if out of range
const char *wb_get_bank_word(const WordBank *wb, size_t idx) {
    if (!wb || idx >= wb->bank.count) return NULL;
    return wb->bank.items[idx];
}

// get a random word from the flat word bank
const char *wb_get_random_bank_word(const WordBank *wb) {
    if (!wb || wb->bank.count == 0) return NULL;
    return wb->bank.items[(size_t)rand() % wb->bank.count];
}
