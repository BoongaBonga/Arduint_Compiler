#ifndef ARDUINT_DEBUG_H
#define ARDUINT_DEBUG_H

#include "chunk.h"
#include "collection.h"
#include "token.h"

#define COL_BLACK "\x1b[30m"
#define COL_RED "\x1b[31m"
#define COL_GREEN "\x1b[32m"
#define COL_YELLOW "\x1b[33m"
#define COL_BLUE "\x1b[34m"
#define COL_MAGENTA "\x1b[35m"
#define COL_CYAN "\x1b[36m"
#define COL_WHITE "\x1b[37m"
#define AC_NORMAL "\x1b[m"

#define BG_BLACK "\x1b[40m"
#define BG_RED "\x1b[41m"
#define BG_GREEN "\x1b[42m"
#define BG_YELLOW "\x1b[43m"
#define BG_BLUE "\x1b[44m"
#define BG_MAGENTA "\x1b[45m"
#define BG_CYAN "\x1b[46m"
#define BG_WHITE "\x1b[47m"

void printToken(Token *token);
void printTokens(Vector *tokens);

int printInstruction(Chunk *chunk, int offset);
void printChunk(Chunk *chunk, const char *name);

void printLexeme(Token *token);
void printValue(Value *value);

#endif