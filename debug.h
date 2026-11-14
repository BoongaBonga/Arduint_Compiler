#ifndef ARDUINT_DEBUG_H
#define ARDUINT_DEBUG_H

#include "chunk.h"
#include "collection.h"
#include "token.h"

void printToken(Token *token);
void printTokens(Vector *tokens);

int printInstruction(Chunk *chunk, int offset);
void printChunk(Chunk *chunk, const char *name);

void printLexeme(Token *token);
void printValue(Value *value);

#endif