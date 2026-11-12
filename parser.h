#ifndef ARDUINT_PARSER_H
#define ARDUINT_PARSER_H

#include <stdbool.h>

#include "chunk.h"
#include "collection.h"
#include "common.h"
#include "token.h"

typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

typedef struct {
  Vector *tokens;
  int currentIndex;
  Chunk chunk;
  Vector breaksToPatch;   // jumps that still need to be patched
  Vector continueIndices; // Continue points to patch new continue statements

  bool hadError;
  bool isInLoop;

  Table stringPool;
  Table globalStrings;

  Compiler compiler;
} Parser;

Parser initParser(Vector *tokens);
void freeParser(Parser *parser);

void parse(Parser *parser);

#endif