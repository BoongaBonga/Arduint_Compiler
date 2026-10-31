#ifndef ARDUINT_LEXER_H
#define ARDUINT_LEXER_H

#include "common.h"
#include "token.h"

typedef struct {
  const char *origin;    // The pointer to the first character
  const char *start;     // The start of the current token
  const char *current;   // The current Token we're going to lex
  int line;              // The line we're on
  const char *lineStart; // The start of the current line, to get offset
  Vector tokens;         // The tokens scanned
} Scanner;

char *copySourceStr(const char *start, int length);

Scanner initScanner(const char *source);

void freeScanner(Scanner *lexer);

Token scanToken(Scanner *lexer);

void scanTokens(Scanner *lexer);

#endif