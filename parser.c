#ifndef ARDUINT_PARSER_C
#define ARDUINT_PARSER_C

#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "errorHandling.h"
#include "lexer.h"
#include "parser.h"
#include "value.h"

typedef enum {
  NODE_PROGRAM,    // S -> E
  NODE_EXPRESSION, // E -> A
  NODE_ASSIGN,     // A -> C | NAME = E
  NODE_COMPARISON, // C -> M | C == M
  NODE_SUM,        // M -> F | M + F
  NODE_FACTOR,     // F -> U | F * U
  NODE_UNARY,      // U -> P | -U | !U
  NODE_POWER,      // P -> G | G ^ P
  NODE_GROUPING,   // G -> A | ( E )
  NODE_ATOM,       // A -> V | NAME
} NodeType;

typedef enum { ATOM_NAME, ATOM_VALUE } AtomType;

extern const char *lexemes;

Parser initParser(Vector *tokens) {
  Parser parser;
  parser.currentIndex = 0;
  parser.tokens = tokens;
  parser.chunk = initChunk();
  parser.hadError = false;
  return parser;
}

void freeParser(Parser *parser) { freeChunk(&parser->chunk); }

inline static Token *peek(Parser *parser) {
  return vecGet(parser->tokens, parser->currentIndex);
}

static Token *next(Parser *parser) {
  if (peek(parser)->type == TOKEN_EOF) return peek(parser);
  return vecGet(parser->tokens, parser->currentIndex++);
}

static bool consume(Parser *parser, TokenType expected, Token *errorStart,
                    const char *message) {
  Token *current = peek(parser);
  if (current->type == expected) {
    next(parser);
    return true;
  }

  customError(errorStart->line, errorStart->offset, current->line,
              current->offset, message);

  parser->hadError = true;
  next(parser);
  return false;
}

static void errorUnexpectedToken(Token *token) {
  char *src = copySourceStr(token->start, token->length);
  errorFromToken(token, "Unexpected token '%s'", src);
  free(src);
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

static void value(Parser *parser, Token *val) {
  Value value;
  switch (val->type) {
  case TOKEN_NUMBER: {
    value = number(val->start, val->length);
    break;
  }
  case TOKEN_TRUE: {
    value.type = BOOL_VAL;
    value.as.boolean = true;
    break;
  }
  case TOKEN_FALSE: {
    value.type = BOOL_VAL;
    value.as.boolean = false;
    break;
  }
  default: {
    char *source = copySourceStr(val->start, val->length);
    errorFromToken(val, "Cannot parse a value from '%s'", source);
    free(source);
    parser->hadError = true;
    return;
  }
  }

  // If the constant offset is bigger than 255, switch to 2-byte adresses
  if (parser->chunk.constants.count < 256) {
    writeOperation(&parser->chunk, OP_CONSTANT_8);
    writeOperation(&parser->chunk, parser->chunk.constants.count);
  } else if (parser->chunk.constants.count < UINT16_MAX) {
    writeOperation(&parser->chunk, OP_CONSTANT_16);
    uint16_t index = parser->chunk.constants.count;
    writeOperation(&parser->chunk, (uint8_t)(index >> 8));
    writeOperation(&parser->chunk, (uint8_t)index);
  } else {
    errorFromToken(val, "Too many constants.");
  }

  writeConstant(&parser->chunk, value);
}

typedef enum { LHS, RHS } ReturnSide;
static int infixBindingPower(TokenType op, ReturnSide retRhs) {
  switch (op) {
  case TOKEN_PLUS:
  case TOKEN_MINUS: return NODE_SUM * 2 + retRhs;
  case TOKEN_STAR:
  case TOKEN_SLASH: return NODE_FACTOR * 2 + retRhs;
  case TOKEN_POWER: return NODE_POWER * 2 + !retRhs;
  default:          return -1;
  }
}

static int prefixBindingPower(TokenType op) {
  switch (op) {
  case TOKEN_MINUS:
  case TOKEN_NOT:
  case TOKEN_PLUS:               return NODE_UNARY * 2;
  case TOKEN_LEFT_ROUND_BRACKET: return NODE_GROUPING * 2;
  default:                       return -1;
  }
}

typedef enum { PREFIX, INFIX } Position;
static uint8_t getOp(TokenType type, Position infix) {
  switch (type) {
  case TOKEN_PLUS:  return infix ? OP_ADD : OP_NO_OP;
  case TOKEN_MINUS: return infix ? OP_SUBTRACT : OP_NEGATE;
  case TOKEN_STAR:  return infix ? OP_MULTIPLY : -1;
  case TOKEN_SLASH: return infix ? OP_DIVIDE : -1;
  case TOKEN_POWER: return infix ? OP_POWER : -1;
  case TOKEN_NOT:   return infix ? -1 : OP_NOT;
  default:          return -1;
  }
}

static void expressionBp(Parser *parser, int minBp) {
  Token *current = next(parser);

  switch (current->type) {
    // Values
  case TOKEN_NUMBER:
  case TOKEN_STRING:
  case TOKEN_FALSE:
  case TOKEN_TRUE:   value(parser, current); break;
  // Infix
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_NOT:    {
    int rightBindingPower = prefixBindingPower(current->type);
    expressionBp(parser, rightBindingPower);

    writeOperation(&parser->chunk, getOp(current->type, PREFIX));
    break;
  }
  case TOKEN_LEFT_ROUND_BRACKET: {
    expressionBp(parser, 0);
    consume(parser, TOKEN_RIGHT_ROUND_BRACKET, current,
            "Found no closing parentheses after '('.");
    break;
  }
  case TOKEN_EOF: return;
  default:        {
    // Report error and try parsing the next token
    errorUnexpectedToken(current);
    parser->hadError = true;
    return;
  }
  }

  for (;;) {
    Token *current = peek(parser);
    if (current->type == TOKEN_EOF) break;

    TokenType op = current->type;
    int lBindingPower = infixBindingPower(op, LHS);
    int rBindingPower = infixBindingPower(op, RHS);

    if (lBindingPower < minBp || lBindingPower == -1) break;

    next(parser);

    expressionBp(parser, rBindingPower);

    writeOperation(&parser->chunk, getOp(current->type, INFIX));
  }
}

static void expression(Parser *parser) { expressionBp(parser, 0); }

static void program(Parser *parser) {
  expression(parser);
  writeOperation(&parser->chunk, OP_RETURN);
}

void parse(Parser *parser) { program(parser); }

#endif