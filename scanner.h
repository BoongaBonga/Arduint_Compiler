#ifndef ARDUINT_SCANNER_H
#define ARDUINT_SCANNER_H

#include <stdbool.h>

#include "collection.h"
#include "token.h"

typedef struct {
  const char *origin;       // The pointer to the first character
  const char *start;        // The start of the current token
  const char *current;      // The current Token we're going to lex
  int line;                 // The line we're on
  const char *lineStart;    // The start of the current line, to get offset
  Vector tokens;            // The tokens scanned
  GenericTable definitions; // Maps define's to arrays of tokens
  Table stringPool;         // Used to make some strings for definitions
  Object *objects;          // All objects used
  bool hadError;
} Scanner;

char *copySourceStr(const char *start, int length);

Scanner initScanner(const char *source);

void freeScanner(Scanner *scanner);

Token scanToken(Scanner *scanner, bool canBeDefinition);

void scanTokens(Scanner *scanner);

#endif