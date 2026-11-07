#include "ascii_art.h"
#include <ctype.h>

#define FONT_HEIGHT 5

// --- LETTER FONT DEFINITIONS (HEIGHT = 5) ---

static const char *font_A[FONT_HEIGHT] = {
    "      ",
    "▄█▀█▄ ",
    "██▀██ ",
    "▀▀ ▀▀ ",
    "      "
};

static const char *font_B[FONT_HEIGHT] = {
    "██   ",
    "██▀█▄",
    "██ ██",
    "▀▀▀▀ ",
    "     "
};

static const char *font_C[FONT_HEIGHT] = {
    "     ",
    "▄█▀█▄",
    "██ ▄▄",
    " ▀▀▀ ",
    "     "
};

static const char *font_D[FONT_HEIGHT] = {
    "   ██",
    "▄█▀██",
    "██ ██",
    " ▀▀▀▀",
    "     "
};

static const char *font_E[FONT_HEIGHT] = {
    "     ",
    "▄█▀█▄",
    "██▀▀ ",
    " ▀▀▀ ",
    "     "
};

static const char *font_F[FONT_HEIGHT] = {
    "█▄▀█▄",
    "██▄▄ ",
    "██   ",
    "▀▀   ",
    "     "
};

static const char *font_G[FONT_HEIGHT] = {
    "     ",
    "▄█▀█▄",
    "██ ██",
    " ▀▀██",
    "  ▀▀ "
};

static const char *font_H[FONT_HEIGHT] = {
    "█▄   ",
    "██▀█▄",
    "██ ██",
    "▀▀ ▀▀",
    "     "
};

static const char *font_I[FONT_HEIGHT] = {
    "  ",
    "▀▀",
    "█▄",
    "▀▀",
    "  "
};

static const char *font_J[FONT_HEIGHT] = {
    " ▄█",
    " ██",
    " ██",
    "▀▀▀",
    "   "
};

static const char *font_K[FONT_HEIGHT] = {
    "█▄ ▄▄",
    "██▀█▄",
    "██ ██",
    "▀▀ ▀▀",
    "     "
};

static const char *font_L[FONT_HEIGHT] = {
    "█▄ ",
    "██ ",
    "██ ",
    "▀▀▀",
    "   "
};

static const char *font_M[FONT_HEIGHT] = {
    "        ",
    "██▀█▄▀██",
    "██ ▀▀ ██",
    "▀▀    ▀▀",
    "        "
};

static const char *font_N[FONT_HEIGHT] = {
    "     ",
    "██▀█▄",
    "██ ██",
    "▀▀ ▀▀",
    "     "
};

static const char *font_O[FONT_HEIGHT] = {
    "     ",
    "▄█▀▄▄",
    "██ ██",
    " ▀▀▀ ",
    "     "
};

static const char *font_P[FONT_HEIGHT] = {
    "     ",
    "██▀█▄",
    "██ ██",
    "██▀▀ ",
    "▀▀   "
};

static const char *font_Q[FONT_HEIGHT] = {
    "     ",
    "▄█▀██",
    "██ ██",
    " ▀▀██",
    "   ▀▀"
};

static const char *font_R[FONT_HEIGHT] = {
    "     ",
    "▄█▀▀▄",
    "██   ",
    "▀▀   ",
    "     "
};

static const char *font_S[FONT_HEIGHT] = {
    "     ",
    "▄█▀▀▀",
    " ▀▀█▄",
    "▀▀▀▀ ",
    "     "
};

static const char *font_T[FONT_HEIGHT] = {
    "    ",
    "▄██▄",
    " ██ ",
    " ▀▀ ",
    "    "
};

static const char *font_U[FONT_HEIGHT] = {
    "     ",
    "██ ██",
    "██ ██",
    " ▀▀▀ ",
    "     "
};

static const char *font_V[FONT_HEIGHT] = {
    "     ",
    "██ ██",
    "▐█ █▌",
    "  ▀  ",
    "     "
};

static const char *font_W[FONT_HEIGHT] = {
    "       ",
    "██ ▄ ██",
    "██ █ ██",
    " ▀▀ ▀▀ ",
    "       "
};

static const char *font_X[FONT_HEIGHT] = {
    "     ",
    "██ ██",
    "▄█▀█▄",
    "▀▀ ▀▀",
    "     "
};

static const char *font_Y[FONT_HEIGHT] = {
    "     ",
    "██ ██",
    "██ ██",
    " ▀▀██",
    "  ▀▀ "
};

static const char *font_Z[FONT_HEIGHT] = {
    "     ",
    "▀▀▀█▄",
    "▄█▀▀ ",
    " ▀▀▀▀",
    "     "
};

static const char *font_space[FONT_HEIGHT] = {
    "   ",
    "   ",
    "   ",
    "   ",
    "   "
};

// --- PUBLIC INTERFACE FUNCTIONS ---

int ascii_get_height(void) {
    return FONT_HEIGHT;
}

const char *ascii_get_line(char c, int line_num) {
    if (line_num < 0 || line_num >= FONT_HEIGHT) return "";

    c = (char)toupper((unsigned char)c);

    switch (c) {
        case 'A': return font_A[line_num];
        case 'B': return font_B[line_num];
        case 'C': return font_C[line_num];
        case 'D': return font_D[line_num];
        case 'E': return font_E[line_num];
        case 'F': return font_F[line_num];
        case 'G': return font_G[line_num];
        case 'H': return font_H[line_num];
        case 'I': return font_I[line_num];
        case 'J': return font_J[line_num];
        case 'K': return font_K[line_num];
        case 'L': return font_L[line_num];
        case 'M': return font_M[line_num];
        case 'N': return font_N[line_num];
        case 'O': return font_O[line_num];
        case 'P': return font_P[line_num];
        case 'Q': return font_Q[line_num];
        case 'R': return font_R[line_num];
        case 'S': return font_S[line_num];
        case 'T': return font_T[line_num];
        case 'U': return font_U[line_num];
        case 'V': return font_V[line_num];
        case 'W': return font_W[line_num];
        case 'X': return font_X[line_num];
        case 'Y': return font_Y[line_num];
        case 'Z': return font_Z[line_num];
        default:  return font_space[line_num];
    }
}
