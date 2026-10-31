#ifndef ARDUINT_DEBUG_C
#define ARDUINT_DEBUG_C

#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "errorHandling.h"
#include "token.h"

const char *lexemes[68] = {
    "IDENTIFIER",
    "NUMBER",
    "STRING",

    // Control flow
    "IF",
    "ELSE",
    "WHILE",
    "FOR",
    "LOOP",
    "RETURN",
    "FUNCTION",
    "FALSE",
    "TRUE",

    // Arduino operations
    "SETPIN",
    "ANALOG",
    "READPIN",
    "DELAY",
    "DELAY_MICROS",
    "PRINT",
    "MICROS",
    "MS",
    "S",
    "M",
    "H",

    // Functions & keywords
    "DEFINE",
    "AS",
    "TO",

    // Operators
    "PLUS",
    "MINUS",
    "SLASH",
    "STAR",
    "POWER",
    "AND",
    "NOT",
    "OR",

    // One-character tokens
    "BACKSLASH",
    "COMMA",
    "POINT_COMMA",
    "COLON",
    "DOT",
    "EQUALS",
    "PERCENT",
    "LEFT_SQUARE_BRACKET",
    "RIGHT_SQUARE_BRACKET",
    "LEFT_ROUND_BRACKET",
    "RIGHT_ROUND_BRACKET",
    "LEFT_SQUIGGLY_BRACKET",
    "RIGHT_SQUIGGLY_BRACKET",
    "LESS",
    "GREATER",

    // Two-character tokens
    "EQUALS_EQUALS",
    "GREATER_EQUALS",
    "LESS_EQUALS",
    "NOT_EQUALS",
    "MINUS_EQUALS",
    "PLUS_EQUALS",
    "SLASH_EQUALS",
    "STAR_EQUALS",
    "POWER_EQUALS",
    "AND_EQUALS",
    "OR_EQUALS",
    "PERCENT_EQUALS",

    "PLUS_PLUS",
    "MINUS_MINUS",
    "AND_AND",
    "OR_OR",

    "NEWLINE",
    "ERROR",
    "EOF",
};

void printToken(Token *token) {
  printf("[%s", lexemes[token->type]);

  bool sourceNeeded = token->type == TOKEN_NUMBER ||
                      token->type == TOKEN_IDENTIFIER ||
                      token->type == TOKEN_ERROR || token->type == TOKEN_STRING;

  if (!sourceNeeded) {
    printf("] ");
    if (token->type == TOKEN_NEWLINE) printf("\n");
    return;
  }

  char *characters = malloc(sizeof(char) * token->length + 1);
  if (characters == NULL) {
    defaultError("Ran out of memory.");
    exit(1);
  }
  memcpy(characters, token->start, token->length);
  characters[token->length] = '\0';

  printf(": %s] ", characters);

  free(characters);
}

void printTokens(Vector *tokens) {
  for (int i = 0; i < tokens->count; i++) {
    printToken(vecGet(tokens, i));
  }
  printf("\n");
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int printInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);

  uint8_t instruction = *(uint8_t *)vecGet(&chunk->byteCode, offset);
  switch (instruction) {
  case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
  default: defaultError("Unknown opCode %d.", instruction); return offset + 1;
  }
}

void printChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->byteCode.count;) {
    offset = printInstruction(chunk, offset);
  }
}

#endif