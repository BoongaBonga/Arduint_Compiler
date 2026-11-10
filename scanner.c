#ifndef ARDUINT_SCANNER_C
#define ARDUINT_SCANNER_C

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collection.h"
#include "errorHandling.h"
#include "scanner.h"

static void defineStatement(Scanner *scanner);

static bool isAlphabetic(char c) {
  return (c == '_') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}
static bool isNumeric(char c) { return ('0' <= c && c <= '9'); }

Scanner initScanner(const char *source) {
  Scanner scanner;
  scanner.start = source;
  scanner.current = source;
  scanner.origin = source;
  scanner.line = 1;
  scanner.lineStart = source;
  scanner.tokens = initVector(sizeof(Token));
  scanner.definitions = initGenericTable(sizeof(Vector));
  scanner.stringPool = initTable();
  return scanner;
}

void freeScanner(Scanner *scanner) {
  for (int i = 0; i < scanner->definitions.capacity; i++) {
    GenericEntry *entry = &scanner->definitions.entries[i];
    if (entry->key != NULL) freeVector(entry->value);
  }
  freeGenericTable(&scanner->definitions);
  freeTable(&scanner->stringPool);
  freeVector(&scanner->tokens);
}

static bool isAtEnd(Scanner *scanner) { return *scanner->current == '\0'; }
/// @brief returns true if we're one char away from EOF
static bool isBeforeEnd(Scanner *scanner) {
  return *(scanner->current + 1) == '\0';
}

static char peek(Scanner *scanner) { return *scanner->current; }

static char peekNext(Scanner *scanner) {
  if (isAtEnd(scanner)) return '\0';
  return *(scanner->current + 1);
}

static char advance(Scanner *scanner) {
  scanner->current++;
  return scanner->current[-1];
}

/// @brief Tries consuming a character, returns true if succesfull.
static bool match(const char c, Scanner *scanner) {
  if (isAtEnd(scanner)) return false;
  if (peek(scanner) != c) return false;

  scanner->current++;
  return true;
}

static void newLine(Scanner *scanner) {
  scanner->line++;
  scanner->lineStart = scanner->current;
}

static void skipWhiteSpace(Scanner *scanner) {
  for (;;) {
    scanner->start = scanner->current;
    const int startLine = scanner->line;
    const char *startLineStart = scanner->lineStart;

    switch (peek(scanner)) {
    case ' ':
    case '\r':
    case '\t': advance(scanner); break;
    case '/':  {
      // Check for comments
      if (peekNext(scanner) == '/') {
        while (!isAtEnd(scanner) && peek(scanner) != '\n') advance(scanner);
        break;
      } else if (peekNext(scanner) == '*') {
        // Multi-line comments
        while (!(peek(scanner) == '*' && peekNext(scanner) == '/')) {
          // Skip chars while incrementing the line upon \n's
          if (advance(scanner) == '\n') newLine(scanner);
          if (!isBeforeEnd(scanner)) continue;

          // If the comment is unterminated, report an error
          customError(startLine, scanner->start - startLineStart, scanner->line,
                      scanner->current - scanner->lineStart + 1,
                      "Unterminated comment.");

          scanner->current += 2;
          return;
        }
        scanner->current += 2; // Skip trailing */
        break;
      }
    }
    default: return;
    }
  }
}

/// @brief Returns a string copied from the source code. Don't forget to free
/// pointer!
char *copySourceStr(const char *start, int length) {
  char *str = malloc(length + 1);
  str[length] = '\0';
  return memcpy(str, start, length);
}

static Token makeToken(Scanner *scanner, TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner->start;
  token.length = (int)(scanner->current - scanner->start);
  token.line = scanner->line;
  token.offset = scanner->start - scanner->lineStart;
  return token;
}

static Token errorToken(Scanner *scanner, const char *message, int line,
                        const char *lineStart, const char *errorStart) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = line;
  token.offset = errorStart - lineStart;

  ptrdiff_t endOffset = scanner->current - scanner->lineStart;

  char *badToken =
      copySourceStr(errorStart, (int)(scanner->current - errorStart));

  customError(line, token.offset, scanner->line, endOffset, message, badToken);

  free(badToken);
  return token;
}

static TokenType checkKeyword(Scanner *scanner, int start, int lengthRemainder,
                              const char *rest, TokenType type) {
  // Check if the length of the token matches and then check characters
  if (scanner->current - scanner->start == start + lengthRemainder &&
      memcmp(scanner->start + start, rest, lengthRemainder) == 0)
    return type;
  else
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner *scanner) {
  const ptrdiff_t identifierLen = scanner->current - scanner->start;
  switch (*scanner->start) {
  case 'A': {
    // If this token is an analog pin, emit a token for the A and the number.
    if (identifierLen == 2 && isNumeric(*(scanner->current - 1))) {
      // Backtrack so the number will be cought next turn
      scanner->current--;
      return TOKEN_ANALOG;
    }
    if (identifierLen == 2) return checkKeyword(scanner, 1, 1, "s", TOKEN_AS);
    return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND_AND);
  }
  case 'a': {
    if (identifierLen == 3)
      return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND_AND);
    return checkKeyword(scanner, 1, 1, "s", TOKEN_AS);
  }
  case 'D':
  case 'd': {
    switch (identifierLen) {
    case 6: {
      TokenType type = checkKeyword(scanner, 1, 5, "efine", TOKEN_DEFINE);
      if (type != TOKEN_DEFINE) return type;
      // Continue scanning the rest of the line
      defineStatement(scanner);
      skipWhiteSpace(scanner);
      return TOKEN_NEWLINE;
    }
    case 11:
      return checkKeyword(scanner, 1, 10, "elayMicros", TOKEN_DELAY_MICROS);
    default: return checkKeyword(scanner, 1, 4, "elay", TOKEN_DELAY);
    }
  }
  case 'E':
  case 'e': {
    if (identifierLen == 4)
      return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
    return checkKeyword(scanner, 1, 5, "quals", TOKEN_EQUALS_EQUALS);
  }
  case 'F':
  case 'f': {
    switch (*(scanner->start + 1)) {
    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
    case 'u': return checkKeyword(scanner, 2, 6, "nction", TOKEN_FUNCTION);
    }
  }
  case 'H': return checkKeyword(scanner, 1, 3, "IGH", TOKEN_TRUE);
  case 'h':
    if (identifierLen == 1) return TOKEN_H;
  case 'I':
  case 'i': return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
  case 'L': {
    if (identifierLen == 3)
      return checkKeyword(scanner, 1, 2, "OW", TOKEN_FALSE);
    return checkKeyword(scanner, 1, 3, "oop", TOKEN_LOOP);
  }
  case 'l': return checkKeyword(scanner, 1, 3, "oop", TOKEN_LOOP);
  case 'm': {
    if (identifierLen == 1) return TOKEN_M;
    if (identifierLen == 6)
      return checkKeyword(scanner, 1, 5, "icros", TOKEN_µS);
    return checkKeyword(scanner, 1, 1, "s", TOKEN_MS);
  }
  case 'N':
  case 'n': {
    if (identifierLen == 3) return checkKeyword(scanner, 1, 2, "ot", TOKEN_NOT);
    return checkKeyword(scanner, 1, 3, "ull", TOKEN_NULL);
  }
  case 'O':
  case 'o': return checkKeyword(scanner, 1, 1, "r", TOKEN_OR_OR);
  case 'P':
  case 'p': return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
  case 'R':
  case 'r': {
    if (identifierLen == 6)
      return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
    return checkKeyword(scanner, 1, 6, "eadPin", TOKEN_READPIN);
  }
  case 'S': return checkKeyword(scanner, 1, 5, "etPin", TOKEN_SETPIN);
  case 's': {
    if (identifierLen == 1) return TOKEN_S;
    return checkKeyword(scanner, 1, 5, "etPin", TOKEN_SETPIN);
  }
  case 'T':
  case 't': {
    if (identifierLen == 2) return checkKeyword(scanner, 1, 1, "o", TOKEN_TO);
    if (checkKeyword(scanner, 1, 3, "rue", TOKEN_TRUE) == TOKEN_TRUE)
      return TOKEN_TRUE;
    return checkKeyword(scanner, 1, 3, "hen", TOKEN_THEN);
  }
  case 'W':
  case 'w': return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
  case 'X':
  case 'x': return checkKeyword(scanner, 1, 2, "or", TOKEN_XOR);
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *scanner) {
  while (isAlphabetic(peek(scanner)) || isNumeric(peek(scanner)))
    advance(scanner);

  return makeToken(scanner, identifierType(scanner));
}

static Token number(Scanner *scanner) {
  while (isNumeric(peek(scanner))) advance(scanner);
  if (!match('.', scanner)) return makeToken(scanner, TOKEN_NUMBER);

  if (!isNumeric(peek(scanner))) {
    return errorToken(scanner, "Unterminated float.", scanner->line,
                      scanner->lineStart, scanner->start);
  }

  while (isNumeric(peek(scanner))) advance(scanner);
  return makeToken(scanner, TOKEN_NUMBER);
}

static Token string(Scanner *scanner) {
  const char *stringStart = scanner->start;
  int startLine = scanner->line;
  const char *startLineStart = scanner->lineStart;

  while (peek(scanner) != '"' && !isAtEnd(scanner)) {
    char c = advance(scanner);
    if (c == '\n') newLine(scanner);
  }
  if (isAtEnd(scanner))
    return errorToken(scanner, "Unterminated string.", startLine,
                      startLineStart, stringStart);

  advance(scanner); // Trailing "
  return makeToken(scanner, TOKEN_STRING);
}

Token scanToken(Scanner *scanner, bool canBeDefinition) {
  skipWhiteSpace(scanner);
  scanner->start = scanner->current;

  if (isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);

  char c = advance(scanner);

  // Check for the 'µ' in 'µs', this is outside of char boundary
  if ((unsigned char)c == 0xC2 && (unsigned char)advance(scanner) == 0xB5) {
    return makeToken(scanner, TOKEN_µS);
  }

  // Check if it's a macro definition
  if (isAlphabetic(c)) {
    Token id = identifier(scanner);
    if (!canBeDefinition || id.type != TOKEN_IDENTIFIER) return id;
    ObjString *name = copyStringVal(id.start, id.length, &scanner->stringPool);

    Vector definition;
    if (!genericTableGet(&scanner->definitions, name, &definition)) return id;

    // Push the defined tokens instead
    for (int i = 0; i < definition.count - 1; i++) {
      Token token = *(Token *)vecGet(&definition, i);
      vecPush(&scanner->tokens, &token);
    }
    // Return the last token
    Token token = *(Token *)vecGet(&definition, definition.count - 1);
    return token;
  } else if (isNumeric(c)) {
    return number(scanner);
  }

#define RET_DOUBLE_TOKEN(token)                                                \
  {                                                                            \
    advance(scanner);                                                          \
    return makeToken(scanner, token);                                          \
  }

  switch (c) {
  case '\n': {
    newLine(scanner);
    return makeToken(scanner, TOKEN_NEWLINE);
  }
  case '\\': return makeToken(scanner, TOKEN_BACKSLASH);
  case '+':  {
    switch (peek(scanner)) {
    case '+': {
      advance(scanner);
      errorToken(scanner, "'++' is not a valid operation.", scanner->line,
                 scanner->lineStart, scanner->current - 2);
      break;
    }
    case '=': RET_DOUBLE_TOKEN(TOKEN_PLUS_EQUALS);
    default:  return makeToken(scanner, TOKEN_PLUS);
    }
  }
  case '-': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_MINUS_EQUALS);
    default:  return makeToken(scanner, TOKEN_MINUS);
    }
  }
  case '*': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_STAR_EQUALS);
    default:  return makeToken(scanner, TOKEN_STAR);
    }
  }
  case '/': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_SLASH_EQUALS);
    default:  return makeToken(scanner, TOKEN_SLASH);
    }
  }
  case '^': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_POWER_EQUALS);
    default:  return makeToken(scanner, TOKEN_POWER);
    }
  }
  case '&': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_AND_EQUALS);
    case '&': RET_DOUBLE_TOKEN(TOKEN_AND_AND);
    default:  return makeToken(scanner, TOKEN_AND);
    }
  }
  case '|': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_OR_EQUALS);
    case '|': RET_DOUBLE_TOKEN(TOKEN_OR_OR);
    default:  return makeToken(scanner, TOKEN_OR);
    }
  }
  case '!': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_NOT_EQUALS);
    default:  return makeToken(scanner, TOKEN_NOT);
    }
  }
  case '=': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_EQUALS_EQUALS);
    default:  return makeToken(scanner, TOKEN_EQUALS);
    }
  }
  case '>': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_GREATER_EQUALS);
    default:  return makeToken(scanner, TOKEN_GREATER);
    }
  }
  case '<': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_LESS_EQUALS);
    default:  return makeToken(scanner, TOKEN_LESS);
    }
  }
  case '%': {
    switch (peek(scanner)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_PERCENT_EQUALS);
    default:  return makeToken(scanner, TOKEN_PERCENT);
    }
  }
  case ',': return makeToken(scanner, TOKEN_COMMA);
  case '.': return makeToken(scanner, TOKEN_DOT);
  case ';': return makeToken(scanner, TOKEN_POINT_COMMA);
  case ':': return makeToken(scanner, TOKEN_COLON);
  case '(': return makeToken(scanner, TOKEN_LEFT_ROUND_BRACKET);
  case ')': return makeToken(scanner, TOKEN_RIGHT_ROUND_BRACKET);
  case '[': return makeToken(scanner, TOKEN_LEFT_SQUARE_BRACKET);
  case ']': return makeToken(scanner, TOKEN_RIGHT_SQUARE_BRACKET);
  case '{': return makeToken(scanner, TOKEN_LEFT_SQUIGGLY_BRACKET);
  case '}': return makeToken(scanner, TOKEN_RIGHT_SQUIGGLY_BRACKET);
  case '"': return string(scanner);
  }
  return errorToken(scanner, "Unexpected token.", scanner->line,
                    scanner->lineStart, scanner->start);

#undef RET_DOUBLE_TOKEN
}

static void errorAndSkipToNewline(Scanner *scanner, const char *message) {
  errorToken(scanner, message, scanner->line, scanner->lineStart,
             scanner->current);
  while (!isAtEnd(scanner)) {
    char last = advance(scanner);
    if (last == '\n') break;
  }
  newLine(scanner);
}

static void defineStatement(Scanner *scanner) {
  skipWhiteSpace(scanner);

  if (!isAlphabetic(peek(scanner))) {
    errorAndSkipToNewline(scanner,
                          "A definition name cannot start with a number.");
    return;
  }

  Token token = identifier(scanner);
  if (token.type != TOKEN_IDENTIFIER) {
    errorAndSkipToNewline(scanner, "Identifier is an Arduint keyword.");
    return;
  }

  // Consume 'as'
  Token asToken = scanToken(scanner, false);
  if (asToken.type != TOKEN_AS) {
    errorAndSkipToNewline(
        scanner,
        "Expected 'as', followed by definition after definition name.");
    return;
  }

  // Scan the remaining statements
  Vector tokenVec = initVector(sizeof(Token));
  for (;;) {
    Token token = scanToken(scanner, false);
    if (token.type == TOKEN_NEWLINE) break;

    if (token.type == TOKEN_POINT_COMMA) continue;

    if (token.type == TOKEN_IDENTIFIER) {
      ObjString *referencedName =
          copyStringVal(token.start, token.length, &scanner->stringPool);
      Vector referencedDefinition;
      if (genericTableGet(&scanner->definitions, referencedName,
                          &referencedDefinition)) {
        for (unsigned int i = 0; i < referencedDefinition.count; i++) {
          vecPush(&tokenVec, vecGet(&referencedDefinition, i));
        }
        continue;
      }
    }

    vecPush(&tokenVec, &token);
  }

  // Make a new entry in the definition table
  ObjString *name =
      copyStringVal(token.start, token.length, &scanner->stringPool);
  genericTableSet(&scanner->definitions, name, &tokenVec);
}

void scanTokens(Scanner *scanner) {
  for (;;) {
    Token token = scanToken(scanner, true);
    vecPush(&scanner->tokens, &token);

    if (token.type == TOKEN_EOF) return;
  }
}

#endif