#ifndef ARDUINT_PARSER_C
#define ARDUINT_PARSER_C

#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "common.h"
#include "errorHandling.h"
#include "lexer.h"
#include "parser.h"
#include "value.h"

Parser initParser(Vector *tokens) {
  Parser parser;
  parser.currentIndex = 0;
  parser.tokens = tokens;
  parser.chunk = initChunk();
  return parser;
}

void freeParser(Parser *parser) { freeChunk(&parser->chunk); }

inline static Token *peek(Parser *parser) {
  return vecGet(parser->tokens, parser->currentIndex);
}

static Token *consume(Parser *parser) {
  if (peek(parser)->type == TOKEN_EOF)
    return peek(parser); // TODO: check if this is stil neccesary
  return vecGet(parser->tokens, parser->currentIndex++);
}

static Value number(const char *start, int length) {
  Value value;
  value.type = NUMBER_VAL;

  char *numStr = copySourceStr(start, length);

  char *endPtr;
  value.as.number = strtod(numStr, &endPtr);

  free(numStr);

  return value;
}

static void value(Parser *parser) {
  Token *current = peek(parser);

  switch (peek(parser)->type) {
  case TOKEN_NUMBER:
    Value num = number(current->start, current->length);
    writeConstant(&parser->chunk, num);
    break;
  default: {
    char *source = copySourceStr(current->start, current->length);
    errorFromToken(current, "Cannot parse a value from '%s'", source);
    free(source);
    break;
  }
  }
}

void parse(Parser *parser) {
  value(parser);
  Value *valPtr = (Value *)vecGet(&parser->chunk.constants, 0);
  printf("Parsed: %lf\n", valPtr->as.number);
  // program(parser);
}

#endif