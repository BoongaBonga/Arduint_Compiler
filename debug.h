#ifndef ARDUINT_DEBUG_H
#define ARDUINT_DEBUG_H

#include "chunk.h"
#include "common.h"
#include "expression.h"
#include "token.h"

void printToken(Token *token);
void printTokens(Vector *tokens);

// void printChunk(Chunk *chunk);

void printProgramTree(ProgramNode *program);

#endif