#ifndef ARDUINT_PARSER_C
#define ARDUINT_PARSER_C

#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "collection.h"
#include "debug.h"
#include "errorHandling.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "value.h"

typedef enum {
  PREC_NONE,
  PREC_PROGRAM,    // S -> E
  PREC_EXPRESSION, // E -> A
  PREC_ASSIGN,     // A -> O | NAME = E
  PREC_L_OR,       // O -> N | O or N
  PREC_L_AND,      // N -> C | N and C
  PREC_COMPARISON, // C -> R | C == R
  PREC_B_OR,       // R -> X | R '|' X
  PREC_B_XOR,      // X -> D | X xor D
  PREC_B_AND,      // D -> M | D '&' M
  PREC_SUM,        // M -> F | M + F
  PREC_FACTOR,     // F -> U | F * U
  PREC_UNARY,      // U -> P | -U | !U
  PREC_POWER,      // P -> T | G ^ P
  PREC_POSTFIX,    // T -> G | T++
  PREC_GROUPING,   // G -> A | ( E )
  PREC_ATOM,       // A -> V | NAME
} Precedence;

typedef enum { ATOM_NAME, ATOM_VALUE } AtomType;

typedef enum { PREFIX, INFIX, POSTFIX } Position;
typedef enum { LHS, RHS } ReturnSide;

typedef struct {
  OpCode prefix;
  OpCode infix;
  OpCode postfix;
} ParseRule;

extern const char *lexemes;

Parser initParser(Vector *tokens) {
  Parser parser;
  parser.currentIndex = 0;
  parser.tokens = tokens;
  parser.chunk = initChunk();
  parser.hadError = false;
  parser.stringPool = initTable();
  return parser;
}

void freeParser(Parser *parser) {
  freeChunk(&parser->chunk);
  freeTable(&parser->stringPool);
}

static Token *peek(Parser *parser) {
  return vecGet(parser->tokens, parser->currentIndex);
}

static Token *previous(Parser *parser) {
  if (parser->currentIndex == 0) return NULL;
  return vecGet(parser->tokens, parser->currentIndex - 1);
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

static bool check(Parser *parser, TokenType type) {
  return peek(parser)->type == type;
}

static bool match(Parser *parser, TokenType expected) {
  if (!check(parser, expected)) return false;
  next(parser);
  return true;
}

static void skipToNewline(Parser *parser) {
  while (peek(parser)->type != TOKEN_NEWLINE && peek(parser)->type != TOKEN_EOF)
    next(parser);
}
static void skipWhiteSpace(Parser *parser) {
  TokenType endToken = peek(parser)->type;
  if (endToken == TOKEN_POINT_COMMA) next(parser);
  while (peek(parser)->type == TOKEN_NEWLINE) next(parser);
}

static void emitOp(Parser *parser, OpCode op) {
  writeOperation(&parser->chunk, op);
}
static void emitOps(Parser *parser, OpCode op1, OpCode op2) {
  writeOperation(&parser->chunk, op1);
  writeOperation(&parser->chunk, op2);
}

static void errorUnexpectedToken(Token *token) {
  // The error already got reported by the scanner
  if (token->type == TOKEN_ERROR) return;
  char *src = copySourceStr(token->start, token->length);
  errorFromToken(token, "Unexpected token '%s'", src);
  free(src);
}

static void emit8or16bytes(Parser *parser, uint16_t bytes, OpCode op8,
                           OpCode op16) {
  if (bytes > UINT8_MAX) {
    emitOp(parser, op16);
    emitOps(parser, (uint8_t)(bytes >> 8), (uint8_t)bytes);
  } else {
    emitOps(parser, op8, (uint8_t)bytes);
  }
}

static bool isAssignmentToken(Token *token) {
  return token->type >= TOKEN_EQUALS && token->type <= TOKEN_PERCENT_EQUALS;
}
static bool matchAssignmentToken(Parser *parser) {
  bool isAssignment = isAssignmentToken(peek(parser));
  if (isAssignment) next(parser);
  return isAssignment;
}

static void expressionBp(Parser *parser, int minBp);
static void expression(Parser *parser);
static void statement(Parser *parser);
static void declaration(Parser *parser);

const OpCode tokenOps[69][3] = {
    // Operators
    [TOKEN_PLUS] = {OP_ERROR, OP_ADD, OP_ERROR},
    [TOKEN_MINUS] = {OP_NEGATE, OP_SUBTRACT, OP_ERROR},
    [TOKEN_SLASH] = {OP_ERROR, OP_DIVIDE, OP_ERROR},
    [TOKEN_STAR] = {OP_ERROR, OP_MULTIPLY, OP_ERROR},
    [TOKEN_POWER] = {OP_ERROR, OP_POWER, OP_ERROR},
    [TOKEN_AND] = {OP_ERROR, OP_AND, OP_ERROR},
    [TOKEN_NOT] = {OP_NOT, OP_ERROR, OP_ERROR},
    [TOKEN_OR] = {OP_ERROR, OP_OR, OP_ERROR},
    [TOKEN_XOR] = {OP_ERROR, OP_XOR, OP_ERROR},

    // One-character tokens
    [TOKEN_EQUALS] = {OP_ERROR, OP_ASSIGN, OP_ERROR},
    [TOKEN_PERCENT] = {OP_ERROR, OP_MODULO,
                       OP_ERROR}, // TODO: maybe also postfix

    [TOKEN_LESS] = {OP_ERROR, OP_LESS, OP_ERROR},
    [TOKEN_GREATER] = {OP_ERROR, OP_GREATER, OP_ERROR},

    // Two-character tokens
    [TOKEN_EQUALS_EQUALS] = {OP_ERROR, OP_EQUALS, OP_ERROR},
    [TOKEN_GREATER_EQUALS] = {OP_ERROR, OP_GREATER_OR_EQUAL, OP_ERROR},
    [TOKEN_LESS_EQUALS] = {OP_ERROR, OP_LESS_OR_EQUAL, OP_ERROR},
    [TOKEN_NOT_EQUALS] = {OP_ERROR, OP_NOT_EQUALS, OP_ERROR},

    // Special case, here the infix means what operation has to be done in the
    // middle
    [TOKEN_MINUS_EQUALS] = {OP_ERROR, OP_SUBTRACT, OP_ERROR},
    [TOKEN_PLUS_EQUALS] = {OP_ERROR, OP_ADD, OP_ERROR},
    [TOKEN_SLASH_EQUALS] = {OP_ERROR, OP_DIVIDE, OP_ERROR},
    [TOKEN_STAR_EQUALS] = {OP_ERROR, OP_MULTIPLY, OP_ERROR},
    [TOKEN_POWER_EQUALS] = {OP_ERROR, OP_POWER, OP_ERROR},
    [TOKEN_AND_EQUALS] = {OP_ERROR, OP_AND, OP_ERROR},
    [TOKEN_OR_EQUALS] = {OP_ERROR, OP_OR, OP_ERROR},
    [TOKEN_PERCENT_EQUALS] = {OP_ERROR, OP_MODULO, OP_ERROR},

    [TOKEN_PLUS_PLUS] = {OP_PRE_INCREMENT, OP_ERROR, OP_POST_INCREMENT},
    [TOKEN_MINUS_MINUS] = {OP_PRE_DECREMENT, OP_ERROR, OP_POST_DECREMENT},
    [TOKEN_AND_AND] = {OP_ERROR, OP_AND_AND, OP_ERROR},
    [TOKEN_OR_OR] = {OP_ERROR, OP_OR_OR, OP_ERROR},
};

static uint8_t getOp(TokenType type, Position pos) {
  return tokenOps[type][pos];
}

static uint16_t identifierConstant(Parser *parser, Token *name) {
  return writeConstant(&parser->chunk,
                       OBJECT_VAL(copyStringVal(name->start, name->length,
                                                &parser->stringPool)));
}

static uint16_t parseVariable(Parser *parser, const char *errorMessage) {
  consume(parser, TOKEN_IDENTIFIER, peek(parser), errorMessage);
  return identifierConstant(parser, previous(parser));
}

static void defineVariable(Parser *parser, uint16_t global) {
  emit8or16bytes(parser, global, OP_DEFINE_GLOBAL_8, OP_DEFINE_GLOBAL_16);
}

static void namedVariable(Parser *parser, Token *name, bool canAssign) {
  uint16_t arg = identifierConstant(parser, name);

  if (canAssign && match(parser, TOKEN_EQUALS)) {
    expression(parser);
    emit8or16bytes(parser, arg, OP_SET_GLOBAL_8, OP_SET_GLOBAL_16);
  } else if (canAssign && isAssignmentToken(peek(parser))) {
    TokenType op = next(parser)->type;

    expression(parser);

    // Get global -> add expr -> set global
    emit8or16bytes(parser, arg, OP_GET_GLOBAL_8, OP_GET_GLOBAL_16);
    emitOp(parser, getOp(op, INFIX));
    emit8or16bytes(parser, arg, OP_SET_GLOBAL_8, OP_SET_GLOBAL_16);
  } else {
    emit8or16bytes(parser, arg, OP_GET_GLOBAL_8, OP_GET_GLOBAL_16);
  }
}

static void identifier(Parser *parser, Token *name, bool canAssign) {
  namedVariable(parser, name, canAssign);
}

static Value number(const char *start, int length) {
  char *numStr = copySourceStr(start, length);
  char *endPtr;

  double value = strtod(numStr, &endPtr);
  free(numStr);

  return NUMBER_VAL(value);
}

static void value(Parser *parser, Token *val) {
  Value value;
  switch (val->type) {
  case TOKEN_NUMBER: {
    value = number(val->start, val->length);
    break;
  }
  case TOKEN_TRUE: {
    value = BOOL_VAL(true);
    break;
  }
  case TOKEN_FALSE: {
    value = BOOL_VAL(false);
    break;
  }
  case TOKEN_NULL: {
    value = NULL_VAL;
    break;
  }
  case TOKEN_STRING: {
    value = OBJECT_VAL(
        copyStringVal(val->start + 1, val->length - 2, &parser->stringPool));
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
    emitOps(parser, OP_CONSTANT_8, parser->chunk.constants.count);
  } else if (parser->chunk.constants.count < UINT16_MAX) {
    emitOp(parser, OP_CONSTANT_16);
    uint16_t index = parser->chunk.constants.count;
    emitOps(parser, (uint8_t)(index >> 8), (uint8_t)index);
  } else {
    errorFromToken(val, "Too many constants.");
  }

  writeConstant(&parser->chunk, value);
}

static int infixBindingPower(TokenType op, ReturnSide retRhs) {
  switch (op) {
  case TOKEN_AND_AND:        return PREC_L_AND * 2 + retRhs;
  case TOKEN_OR_OR:          return PREC_L_OR * 2 + retRhs;

  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUALS:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUALS:
  case TOKEN_EQUALS_EQUALS:
  case TOKEN_NOT_EQUALS:     return PREC_COMPARISON * 2 + retRhs;

  case TOKEN_EQUALS:
  case TOKEN_PLUS_EQUALS:
  case TOKEN_OR_EQUALS:
  case TOKEN_AND_EQUALS:
  case TOKEN_STAR_EQUALS:
  case TOKEN_MINUS_EQUALS:
  case TOKEN_POWER_EQUALS:
  case TOKEN_SLASH_EQUALS:
  case TOKEN_PERCENT_EQUALS: return PREC_ASSIGN * 2 + !retRhs;

  case TOKEN_OR:             return PREC_B_OR * 2 + retRhs;
  case TOKEN_XOR:            return PREC_B_XOR * 2 + retRhs;
  case TOKEN_AND:            return PREC_B_AND * 2 + retRhs;

  case TOKEN_PLUS:
  case TOKEN_MINUS:          return PREC_SUM * 2 + retRhs;
  case TOKEN_STAR:
  case TOKEN_PERCENT:
  case TOKEN_SLASH:          return PREC_FACTOR * 2 + retRhs;
  case TOKEN_POWER:          return PREC_POWER * 2 + !retRhs;

  default:                   return -1;
  }
}

static int prefixBindingPower(TokenType op) {
  switch (op) {
  case TOKEN_PLUS_PLUS:
  case TOKEN_MINUS_MINUS:
  case TOKEN_MINUS:
  case TOKEN_NOT:
  case TOKEN_PLUS:               return PREC_UNARY * 2;
  case TOKEN_LEFT_ROUND_BRACKET: return PREC_GROUPING * 2;
  default:                       return -1;
  }
}

static int prefix(Parser *parser, Token *current, int minBp) {
  switch (current->type) {

    // Values
  case TOKEN_NUMBER:
  case TOKEN_STRING:
  case TOKEN_FALSE:
  case TOKEN_TRUE:
  case TOKEN_NULL:
    value(parser, current);
    return 0;

    // Assignment & variables
  case TOKEN_IDENTIFIER: {
    bool canAssign = minBp <= PREC_ASSIGN;
    identifier(parser, current, canAssign);
    return 0;
  }

  // Prefix
  case TOKEN_PLUS: {
    int rightBindingPower = prefixBindingPower(current->type);
    expressionBp(parser, rightBindingPower);
    return 0;
  }
  case TOKEN_MINUS:
  case TOKEN_NOT:   {
    int rightBindingPower = prefixBindingPower(current->type);
    expressionBp(parser, rightBindingPower);

    emitOp(parser, getOp(current->type, PREFIX));
    return 0;
  }

  // Brackets
  case TOKEN_LEFT_ROUND_BRACKET: {
    expressionBp(parser, 0);
    consume(parser, TOKEN_RIGHT_ROUND_BRACKET, current,
            "Found no closing parentheses after '('.");
    return 0;
  }

  default: {
    // Report error and try parsing the next token
    errorUnexpectedToken(current);
    parser->hadError = true;
    return -1;
  }
  }
}

static int infix(Parser *parser, Token *current, int minBp) {
  TokenType op = current->type;
  int lBindingPower = infixBindingPower(op, LHS);
  int rBindingPower = infixBindingPower(op, RHS);

  if (lBindingPower < minBp || lBindingPower == -1) return 1;

  next(parser);

  expressionBp(parser, rBindingPower);

  emitOp(parser, getOp(current->type, INFIX));
}

static void expressionBp(Parser *parser, int minBp) {
  Token *current = next(parser);

  if (current->type == TOKEN_EOF) return;
  if (prefix(parser, current, minBp) == -1) return;

  for (;;) {
    Token *current = peek(parser);
    if (current->type == TOKEN_EOF) break;

    if (infix(parser, current, minBp) == 1) break;
  }

  // Catch equality silently returning
  bool canAssign = minBp <= PREC_ASSIGN;
  if (!canAssign && isAssignmentToken(peek(parser))) {
    // Reuse the current token at the start of the expression
    errorFromToken(peek(parser), "Invalid assignment target.");
    parser->hadError = true;
    next(parser);
  }
}

static void expression(Parser *parser) { expressionBp(parser, 0); }

static void printStatement(Parser *parser) {
  // Consume 'print'
  consume(parser, TOKEN_PRINT, peek(parser),
          "Tried parsing a print statement without a print token.");

  expression(parser);
  emitOp(parser, OP_PRINT);
}

static void expressionStatement(Parser *parser) {
  expression(parser);
  emitOp(parser, OP_POP);
}

static void synchronize(Parser *parser) {
  parser->hadError = false;
  while (peek(parser)->type != TOKEN_EOF) {
    if (peek(parser)->type == TOKEN_POINT_COMMA ||
        peek(parser)->type == TOKEN_NEWLINE)
      break; // break out of loop & skip whitespace

    switch (peek(parser)->type) {
    case TOKEN_FUNCTION:
    case TOKEN_PRINT:
    case TOKEN_DEFINE:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_LOOP:
    case TOKEN_RETURN:
    case TOKEN_SETPIN:
    case TOKEN_READPIN:
    case TOKEN_DELAY:
    case TOKEN_DELAY_MICROS: return; // Found new statement start
    default:                 next(parser); continue;
    }
  }
  skipWhiteSpace(parser);
}

static void statement(Parser *parser) {
  Token *current = peek(parser);

  switch (current->type) {
  case TOKEN_PRINT: printStatement(parser); break;
  case TOKEN_POINT_COMMA:
    // Report error, logic underneath will deal with cleanup
    errorFromToken(current, "Expected statement, but got ';'.");
    break;
  default: expressionStatement(parser); break;
  }
}

static void declaration(Parser *parser) {
  statement(parser);
  if (parser->hadError) synchronize(parser);
}

static void program(Parser *parser) {
  // Guard against a semicolon on the first line
  if (peek(parser)->type == TOKEN_POINT_COMMA)
    errorFromToken(peek(parser), "Expected statement, but got ';'.");

  while (peek(parser)->type != TOKEN_EOF) {
    skipWhiteSpace(parser);
    if (peek(parser)->type == TOKEN_EOF) break;
    declaration(parser);
  }
  emitOp(parser, OP_RETURN);
}

void parse(Parser *parser) { program(parser); }

#endif