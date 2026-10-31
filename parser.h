#ifndef ARDUINT_PARSER_H
#define ARDUINT_PARSER_H

#include "chunk.h"
#include "token.h"

typedef struct {
  Vector *tokens;
  int currentIndex;
  Chunk chunk;
} Parser;

Parser initParser(Vector *tokens);
void freeParser(Parser *parser);

void parse(Parser *parser);

#endif