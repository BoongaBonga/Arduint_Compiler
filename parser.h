#ifndef ARDUINT_PARSER_H
#define ARDUINT_PARSER_H

#include <stdbool.h>

#include "chunk.h"
#include "collection.h"
#include "token.h"

typedef struct {
  Vector *tokens;
  int currentIndex;
  Chunk chunk;
  bool hadError;
  Table stringPool;
} Parser;

Parser initParser(Vector *tokens);
void freeParser(Parser *parser);

void parse(Parser *parser);

#endif