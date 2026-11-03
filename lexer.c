#ifndef ARDUINT_LEXER_C
#define ARDUINT_LEXER_C

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collection.h"
#include "errorHandling.h"
#include "lexer.h"

static bool isAlphabetic(char c) {
  return (c == '_') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}
static bool isNumeric(char c) { return ('0' <= c && c <= '9'); }

Scanner initScanner(const char *source) {
  Scanner lexer;
  lexer.start = source;
  lexer.current = source;
  lexer.origin = source;
  lexer.line = 1;
  lexer.lineStart = source;
  lexer.tokens = initVector(sizeof(Token));
  return lexer;
}

void freeScanner(Scanner *lexer) { freeVector(&lexer->tokens); }

static bool isAtEnd(Scanner *lexer) { return *lexer->current == '\0'; }
/// @brief returns true if we're one char away from EOF
static bool isBeforeEnd(Scanner *lexer) {
  return *(lexer->current + 1) == '\0';
}

static char peek(Scanner *lexer) { return *lexer->current; }

static char peekNext(Scanner *lexer) {
  if (isAtEnd(lexer)) return '\0';
  return *(lexer->current + 1);
}

static char advance(Scanner *lexer) {
  lexer->current++;
  return lexer->current[-1];
}

/// @brief Tries consuming a character, returns true if succesfull.
static bool match(const char c, Scanner *lexer) {
  if (isAtEnd(lexer)) return false;
  if (peek(lexer) != c) return false;

  lexer->current++;
  return true;
}

static void newLine(Scanner *lexer) {
  lexer->line++;
  lexer->lineStart = lexer->current;
}

static void skipWhiteSpace(Scanner *lexer) {
  for (;;) {
    lexer->start = lexer->current;
    const int startLine = lexer->line;
    const char *startLineStart = lexer->lineStart;

    switch (peek(lexer)) {
    case ' ':
    case '\r':
    case '\t': advance(lexer); break;
    case '/':  {
      // Check for comments
      if (peekNext(lexer) == '/') {
        while (!isAtEnd(lexer) && peek(lexer) != '\n') advance(lexer);
        break;
      } else if (peekNext(lexer) == '*') {
        // Multi-line comments
        while (!(peek(lexer) == '*' && peekNext(lexer) == '/')) {
          // Skip chars while incrementing the line upon \n's
          if (advance(lexer) == '\n') newLine(lexer);
          if (!isBeforeEnd(lexer)) continue;

          // If the comment is unterminated, report an error
          customError(startLine, lexer->start - startLineStart, lexer->line,
                      lexer->current - lexer->lineStart + 1,
                      "Unterminated comment.");

          lexer->current += 2;
          return;
        }
        lexer->current += 2; // Skip trailing */
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

static Token makeToken(Scanner *lexer, TokenType type) {
  Token token;
  token.type = type;
  token.start = lexer->start;
  token.length = (int)(lexer->current - lexer->start);
  token.line = lexer->line;
  token.offset = lexer->current - lexer->lineStart;

  return token;
}

static Token errorToken(Scanner *lexer, const char *message, int line,
                        const char *lineStart, const char *errorStart) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = line;
  token.offset = errorStart - lineStart;

  ptrdiff_t endOffset = lexer->current - lexer->lineStart;

  char *badToken =
      copySourceStr(errorStart, (int)(lexer->current - errorStart));

  customError(line, token.offset, lexer->line, endOffset, message, badToken);

  free(badToken);
  return token;
}

static TokenType checkKeyword(Scanner *lexer, int start, int lengthRemainder,
                              const char *rest, TokenType type) {
  // Check if the length of the token matches and then check characters
  if (lexer->current - lexer->start == start + lengthRemainder &&
      memcmp(lexer->start + start, rest, lengthRemainder) == 0)
    return type;
  else
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner *lexer) {
  const ptrdiff_t identifierLen = lexer->current - lexer->start;
  switch (*lexer->start) {
  case 'A': {
    // If this token is an analog pin, emit a token for the A and the number.
    if (identifierLen == 2 && isNumeric(*(lexer->current - 1))) {
      // Backtrack so the number will be cought next turn
      lexer->current--;
      return TOKEN_ANALOG;
    }
    if (identifierLen == 2) return checkKeyword(lexer, 1, 1, "s", TOKEN_AS);
    return checkKeyword(lexer, 1, 2, "nd", TOKEN_AND_AND);
  }
  case 'a': {
    if (identifierLen == 3)
      return checkKeyword(lexer, 1, 2, "nd", TOKEN_AND_AND);
    return checkKeyword(lexer, 1, 1, "s", TOKEN_AS);
  }
  case 'D':
  case 'd': {
    switch (identifierLen) {
    case 6: return checkKeyword(lexer, 1, 5, "efine", TOKEN_DEFINE);
    case 11:
      return checkKeyword(lexer, 1, 10, "elayMicros", TOKEN_DELAY_MICROS);
    default: return checkKeyword(lexer, 1, 4, "elay", TOKEN_DELAY);
    }
  }
  case 'E':
  case 'e': {
    if (identifierLen == 4) return checkKeyword(lexer, 1, 3, "lse", TOKEN_ELSE);
    return checkKeyword(lexer, 1, 5, "quals", TOKEN_EQUALS_EQUALS);
  }
  case 'F':
  case 'f': {
    switch (*(lexer->start + 1)) {
    case 'a': return checkKeyword(lexer, 2, 3, "lse", TOKEN_FALSE);
    case 'o': return checkKeyword(lexer, 2, 1, "r", TOKEN_FOR);
    case 'u': return checkKeyword(lexer, 2, 6, "nction", TOKEN_FUNCTION);
    }
  }
  case 'H': return checkKeyword(lexer, 1, 3, "IGH", TOKEN_TRUE);
  case 'h':
    if (identifierLen == 1) return TOKEN_H;
  case 'I':
  case 'i': return checkKeyword(lexer, 1, 1, "f", TOKEN_IF);
  case 'L': {
    if (identifierLen == 3) return checkKeyword(lexer, 1, 2, "OW", TOKEN_FALSE);
    return checkKeyword(lexer, 1, 3, "oop", TOKEN_LOOP);
  }
  case 'l': return checkKeyword(lexer, 1, 3, "oop", TOKEN_LOOP);
  case 'm': {
    if (identifierLen == 1) return TOKEN_M;
    if (identifierLen == 6) return checkKeyword(lexer, 1, 5, "icros", TOKEN_µS);
    return checkKeyword(lexer, 1, 1, "s", TOKEN_MS);
  }
  case 'N':
  case 'n': {
    if (identifierLen == 3) return checkKeyword(lexer, 1, 2, "ot", TOKEN_NOT);
    return checkKeyword(lexer, 1, 3, "ull", TOKEN_NULL);
  }
  case 'O':
  case 'o': return checkKeyword(lexer, 1, 1, "r", TOKEN_OR_OR);
  case 'P':
  case 'p': return checkKeyword(lexer, 1, 4, "rint", TOKEN_PRINT);
  case 'R':
  case 'r': {
    if (identifierLen == 6)
      return checkKeyword(lexer, 1, 5, "eturn", TOKEN_RETURN);
    return checkKeyword(lexer, 1, 6, "eadPin", TOKEN_READPIN);
  }
  case 'S': return checkKeyword(lexer, 1, 5, "etPin", TOKEN_SETPIN);
  case 's': {
    if (identifierLen == 1) return TOKEN_S;
    return checkKeyword(lexer, 1, 5, "etPin", TOKEN_SETPIN);
  }
  case 'T':
  case 't': {
    if (identifierLen == 2) return checkKeyword(lexer, 1, 1, "o", TOKEN_TO);
    return checkKeyword(lexer, 1, 3, "rue", TOKEN_TRUE);
  }
  case 'W':
  case 'w': return checkKeyword(lexer, 1, 4, "hile", TOKEN_WHILE);
  case 'X':
  case 'x': return checkKeyword(lexer, 1, 2, "or", TOKEN_XOR);
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *lexer) {
  while (isAlphabetic(peek(lexer)) || isNumeric(peek(lexer))) advance(lexer);
  return makeToken(lexer, identifierType(lexer));
}

static Token number(Scanner *lexer) {
  while (isNumeric(peek(lexer))) advance(lexer);
  if (!match('.', lexer)) return makeToken(lexer, TOKEN_NUMBER);

  if (!isNumeric(peek(lexer))) {
    return errorToken(lexer, "Unterminated float.", lexer->line,
                      lexer->lineStart, lexer->start);
  }

  while (isNumeric(peek(lexer))) advance(lexer);
  return makeToken(lexer, TOKEN_NUMBER);
}

static Token string(Scanner *lexer) {
  const char *stringStart = lexer->start;
  int startLine = lexer->line;
  const char *startLineStart = lexer->lineStart;

  while (peek(lexer) != '"' && !isAtEnd(lexer)) {
    char c = advance(lexer);
    if (c == '\n') newLine(lexer);
  }
  if (isAtEnd(lexer))
    return errorToken(lexer, "Unterminated string.", startLine, startLineStart,
                      stringStart);

  advance(lexer); // Trailing "
  return makeToken(lexer, TOKEN_STRING);
}

Token scanToken(Scanner *lexer) {
  skipWhiteSpace(lexer);
  lexer->start = lexer->current;

  if (isAtEnd(lexer)) return makeToken(lexer, TOKEN_EOF);

  char c = advance(lexer);

  // Check for the 'µ' in 'µs', this is outside of char boundary
  if ((unsigned char)c == 0xC2 && (unsigned char)advance(lexer) == 0xB5) {
    return makeToken(lexer, TOKEN_µS);
  }

  if (isAlphabetic(c)) {
    return identifier(lexer);
  } else if (isNumeric(c)) {
    return number(lexer);
  }

#define RET_DOUBLE_TOKEN(token)                                                \
  {                                                                            \
    advance(lexer);                                                            \
    return makeToken(lexer, token);                                            \
  }

  switch (c) {
  case '\n': {
    newLine(lexer);
    return makeToken(lexer, TOKEN_NEWLINE);
  }
  case '\\': return makeToken(lexer, TOKEN_BACKSLASH);
  case '+':  {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_PLUS_EQUALS);
    case '+': RET_DOUBLE_TOKEN(TOKEN_PLUS_PLUS);
    default:  return makeToken(lexer, TOKEN_PLUS);
    }
  }
  case '-': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_MINUS_EQUALS);
    case '-': RET_DOUBLE_TOKEN(TOKEN_MINUS_MINUS);
    default:  return makeToken(lexer, TOKEN_MINUS);
    }
  }
  case '*': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_STAR_EQUALS);
    default:  return makeToken(lexer, TOKEN_STAR);
    }
  }
  case '/': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_SLASH_EQUALS);
    default:  return makeToken(lexer, TOKEN_SLASH);
    }
  }
  case '^': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_POWER_EQUALS);
    default:  return makeToken(lexer, TOKEN_POWER);
    }
  }
  case '&': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_AND_EQUALS);
    case '&': RET_DOUBLE_TOKEN(TOKEN_AND_AND);
    default:  return makeToken(lexer, TOKEN_AND);
    }
  }
  case '|': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_OR_EQUALS);
    case '|': RET_DOUBLE_TOKEN(TOKEN_OR_OR);
    default:  return makeToken(lexer, TOKEN_OR);
    }
  }
  case '!': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_NOT_EQUALS);
    default:  return makeToken(lexer, TOKEN_NOT);
    }
  }
  case '=': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_EQUALS_EQUALS);
    default:  return makeToken(lexer, TOKEN_EQUALS);
    }
  }
  case '>': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_GREATER_EQUALS);
    default:  return makeToken(lexer, TOKEN_GREATER);
    }
  }
  case '<': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_LESS_EQUALS);
    default:  return makeToken(lexer, TOKEN_LESS);
    }
  }
  case '%': {
    switch (peek(lexer)) {
    case '=': RET_DOUBLE_TOKEN(TOKEN_PERCENT_EQUALS);
    default:  return makeToken(lexer, TOKEN_PERCENT);
    }
  }
  case ',': return makeToken(lexer, TOKEN_COMMA);
  case '.': return makeToken(lexer, TOKEN_DOT);
  case ';': return makeToken(lexer, TOKEN_POINT_COMMA);
  case ':': return makeToken(lexer, TOKEN_COLON);
  case '(': return makeToken(lexer, TOKEN_LEFT_ROUND_BRACKET);
  case ')': return makeToken(lexer, TOKEN_RIGHT_ROUND_BRACKET);
  case '[': return makeToken(lexer, TOKEN_LEFT_SQUARE_BRACKET);
  case ']': return makeToken(lexer, TOKEN_RIGHT_SQUARE_BRACKET);
  case '{': return makeToken(lexer, TOKEN_LEFT_SQUIGGLY_BRACKET);
  case '}': return makeToken(lexer, TOKEN_RIGHT_SQUIGGLY_BRACKET);
  case '"': return string(lexer);
  }
  return errorToken(lexer, "Unexpected token.", lexer->line, lexer->lineStart,
                    lexer->start);

#undef RET_DOUBLE_TOKEN
}

void scanTokens(Scanner *lexer) {
  for (;;) {
    Token token = scanToken(lexer);
    vecPush(&lexer->tokens, &token);

    if (token.type == TOKEN_EOF) return;
  }
}

#endif