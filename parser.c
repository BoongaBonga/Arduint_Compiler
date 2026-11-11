#ifndef ARDUINT_PARSER_C
#define ARDUINT_PARSER_C

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static void initCompiler(Parser *parser) {
  Compiler compiler;
  compiler.localCount = 0;
  compiler.scopeDepth = 0;
  parser->compiler = compiler;
}

Parser initParser(Vector *tokens) {
  Parser parser;
  parser.currentIndex = 0;
  parser.tokens = tokens;
  parser.chunk = initChunk();
  parser.hadError = false;
  parser.stringPool = initTable();
  parser.globalStrings = initTable();
  initCompiler(&parser);
  return parser;
}

void freeParser(Parser *parser) {
  freeChunk(&parser->chunk);
  freeTable(&parser->stringPool);
  freeTable(&parser->globalStrings);
}

Chunk *currentChunk(Parser *parser) { return &parser->chunk; }

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

static void skipWhiteSpace(Parser *parser) {
  while (peek(parser)->type == TOKEN_NEWLINE) next(parser);
}

static bool consume(Parser *parser, TokenType expected, Token *errorStart,
                    const char *message) {
  // Skip newlines
  skipWhiteSpace(parser);

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

static void emitOp(Parser *parser, OpCode op) {
  writeOperation(currentChunk(parser), (uint8_t)op);
}
static void emitOps(Parser *parser, OpCode op1, OpCode op2) {
  writeOperation(currentChunk(parser), op1);
  writeOperation(currentChunk(parser), op2);
}

static void emitLoop(Parser *parser, int loopStart, Token *loopToken) {
  emitOp(parser, OP_LOOP);

  int offset = currentChunk(parser)->byteCode.count - loopStart + 2;
  if (offset > UINT16_MAX) {
    errorFromToken(loopToken, "Loop body too large.");
  }

  emitOps(parser, ((offset >> 8) & 0xff), (offset & 0xff));
}

static int emitJump(Parser *parser, OpCode instruction) {
  emitOp(parser, instruction);
  emitOps(parser, 0xff, 0xff);
  return currentChunk(parser)->byteCode.count - 2;
}

static void patchJump(Parser *parser, int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk(parser)->byteCode.count - offset - 2;

  if (jump > UINT16_MAX) {
    errorFromToken(peek(parser), "Too much code to jump over.");
  }

  *(uint8_t *)vecGet(&currentChunk(parser)->byteCode, offset) =
      (jump >> 8) & 0xff;
  *(uint8_t *)vecGet(&currentChunk(parser)->byteCode, offset + 1) = jump & 0xff;
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

    [TOKEN_AND_AND] = {OP_ERROR, OP_AND_AND, OP_ERROR},
    [TOKEN_OR_OR] = {OP_ERROR, OP_OR_OR, OP_ERROR},
};

static uint8_t getOp(TokenType type, Position pos) {
  return tokenOps[type][pos];
}

static void beginScope(Parser *parser) { parser->compiler.scopeDepth++; }
static void endScope(Parser *parser) {
  parser->compiler.scopeDepth--;

  while (parser->compiler.localCount > 0 &&
         parser->compiler.locals[parser->compiler.localCount - 1].depth >
             parser->compiler.scopeDepth) {
    emitOp(parser, OP_POP);
    parser->compiler.localCount--;
  }
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      return i;
    }
  }

  return -1;
}

static bool globalExists(Parser *parser, Token *name) {
  ObjString *identifierName =
      copyStringVal(name->start, name->length, &parser->stringPool);
  return tableGet(&parser->globalStrings, identifierName, NULL);
}

static uint16_t identifierConstant(Parser *parser, Token *name) {
  // Check whether name is already in globals
  ObjString *identifierName =
      copyStringVal(name->start, name->length, &parser->stringPool);

  Value value;
  bool containsName = tableGet(&parser->globalStrings, identifierName, &value);
  // if it is in glboals, return that index
  if (containsName) return (uint16_t)value.as.number;

  // otherwise make one
  uint16_t index =
      writeConstant(currentChunk(parser), OBJECT_VAL(identifierName));

  value.type = VAL_NUMBER;
  value.as.number = (double)index;
  tableSet(&parser->globalStrings, identifierName, value);
  return index;
}

static void addLocal(Parser *parser, Token name) {
  if (parser->compiler.localCount == UINT8_COUNT) {
    errorFromToken(previous(parser), "Too many local variables in one block.");
    return;
  }

  Local *local = &parser->compiler.locals[parser->compiler.localCount++];
  local->name = name;
  local->depth = parser->compiler.scopeDepth;
}

static void declareVariable(Parser *parser) {
  if (parser->compiler.scopeDepth == 0) return;

  Token *name = previous(parser);

  for (int i = parser->compiler.localCount - 1; i >= 0; i--) {
    Local *local = &parser->compiler.locals[i];
    if (local->depth != -1 && local->depth < parser->compiler.scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      errorFromToken(name, "Already a variable with this name in this scope.");
    }
  }

  addLocal(parser, *name);
}

static uint16_t parseVariable(Parser *parser) {
  // consume(parser, TOKEN_IDENTIFIER, peek(parser), errorMessage);

  declareVariable(parser);
  if (parser->compiler.scopeDepth > 0)
    return (uint16_t)(parser->compiler.localCount - 1);
  return identifierConstant(parser, previous(parser));
}

static void defineVariable(Parser *parser, uint16_t global) {
  if (parser->compiler.scopeDepth > 0) return;

  emitOp(parser, OP_DEFINE_GLOBAL);
}

static void namedVariable(Parser *parser, Token *name, bool canAssign) {
  uint16_t getOperation, setOperation;
  int arg = resolveLocal(&parser->compiler, name);

  // If a local exists, use it
  if (arg != -1) {
    getOperation = OP_GET_LOCAL;
    setOperation = OP_SET_LOCAL;
  } else {
    // Otherwise, if a global exists / we're in global scope, use that.
    if (globalExists(parser, name) || parser->compiler.scopeDepth == 0) {
      arg = identifierConstant(parser, name);
      getOperation = OP_GET_GLOBAL;
      setOperation = OP_SET_GLOBAL;
    } else {
      getOperation = OP_ERROR;
      if (peek(parser)->type != TOKEN_EQUALS)
        errorFromToken(name, "Cannot read from an undeclared variable.");
      setOperation = OP_SET_LOCAL;
      arg = parseVariable(parser);
    }
  }

  if (canAssign && match(parser, TOKEN_EQUALS)) {
    expression(parser);
    emitOps(parser, setOperation, (uint8_t)arg);
  } else if (canAssign && isAssignmentToken(peek(parser))) {
    TokenType op = next(parser)->type;

    expression(parser);

    // Get global -> add expr -> set global
    emitOps(parser, getOperation, (uint8_t)arg);
    emitOp(parser, getOp(op, INFIX));
    emitOps(parser, setOperation, (uint8_t)arg);
  } else {
    emitOps(parser, getOperation, (uint8_t)arg);
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

  writeConstant(currentChunk(parser), value);
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
  case TOKEN_NOT_EQUALS:
    return PREC_COMPARISON * 2 + retRhs;
    /*
      case TOKEN_EQUALS:
      case TOKEN_PLUS_EQUALS:
      case TOKEN_OR_EQUALS:
      case TOKEN_AND_EQUALS:
      case TOKEN_STAR_EQUALS:
      case TOKEN_MINUS_EQUALS:
      case TOKEN_POWER_EQUALS:
      case TOKEN_SLASH_EQUALS:
      case TOKEN_PERCENT_EQUALS: return PREC_ASSIGN * 2 + !retRhs;
    */
  case TOKEN_OR:      return PREC_B_OR * 2 + retRhs;
  case TOKEN_XOR:     return PREC_B_XOR * 2 + retRhs;
  case TOKEN_AND:     return PREC_B_AND * 2 + retRhs;

  case TOKEN_PLUS:
  case TOKEN_MINUS:   return PREC_SUM * 2 + retRhs;
  case TOKEN_STAR:
  case TOKEN_PERCENT:
  case TOKEN_SLASH:   return PREC_FACTOR * 2 + retRhs;
  case TOKEN_POWER:   return PREC_POWER * 2 + !retRhs;

  default:            return -1;
  }
}

static int prefixBindingPower(TokenType op) {
  switch (op) {
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
    warningFromToken(
        current, "Putting a '+' in front of a value is unneccesary and will be "
                 "ignored.");
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
  switch (op) {
  case TOKEN_AND_AND: {
    int lBindingPower = infixBindingPower(TOKEN_AND_AND, LHS);
    int rBindingPower = infixBindingPower(TOKEN_AND_AND, RHS);
    if (lBindingPower < minBp || lBindingPower == -1) return 1;

    next(parser);
    int endJump = emitJump(parser, OP_JUMP_IF_FALSE);

    emitOp(parser, OP_POP);
    expressionBp(parser, rBindingPower);

    patchJump(parser, endJump);
    return 0;
  }
  case TOKEN_OR_OR: {
    int lBindingPower = infixBindingPower(TOKEN_OR_OR, LHS);
    int rBindingPower = infixBindingPower(TOKEN_OR_OR, RHS);
    if (lBindingPower < minBp || lBindingPower == -1) return 1;

    next(parser);
    int elseJump = emitJump(parser, OP_JUMP_IF_FALSE);
    int endJump = emitJump(parser, OP_JUMP);

    patchJump(parser, elseJump);
    emitOp(parser, OP_POP);

    expressionBp(parser, rBindingPower);
    patchJump(parser, endJump);
    return 0;
  }
  default: {
    int lBindingPower = infixBindingPower(op, LHS);
    int rBindingPower = infixBindingPower(op, RHS);

    if (lBindingPower < minBp || lBindingPower == -1) return 1;

    next(parser);

    expressionBp(parser, rBindingPower);

    emitOp(parser, getOp(current->type, INFIX));
    return 0;
  }
  }
}

static void expressionBp(Parser *parser, int minBp) {
  skipWhiteSpace(parser);
  Token *current = next(parser);

  if (current->type == TOKEN_EOF) return;
  if (prefix(parser, current, minBp) == -1) return;

  for (;;) {
    skipWhiteSpace(parser);
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

//____________________________________________
//                STATEMENTS
//____________________________________________

static void block(Parser *parser) {
  consume(parser, TOKEN_LEFT_SQUIGGLY_BRACKET, peek(parser),
          "Cannot start a block without a '{'.");

  while (!check(parser, TOKEN_RIGHT_SQUIGGLY_BRACKET) &&
         !check(parser, TOKEN_EOF)) {
    declaration(parser);
    skipWhiteSpace(parser);
  }

  consume(parser, TOKEN_RIGHT_SQUIGGLY_BRACKET, peek(parser),
          "Excpect '}' after a block.");
}

static void printStatement(Parser *parser) {
  // Consume 'print'
  consume(parser, TOKEN_PRINT, peek(parser),
          "Tried parsing a print statement without a print token.");
  skipWhiteSpace(parser);

  expression(parser);
  emitOp(parser, OP_PRINT);
}

static void ifStatement(Parser *parser) {
  next(parser);

  expression(parser);

  skipWhiteSpace(parser);
  consume(parser, TOKEN_THEN, peek(parser),
          "Expect a 'then' after if condition.");

  int thenJump = emitJump(parser, OP_JUMP_IF_FALSE);
  emitOp(parser, OP_POP);

  statement(parser);

  int elseJump = emitJump(parser, OP_JUMP);

  patchJump(parser, thenJump);
  emitOp(parser, OP_POP);

  skipWhiteSpace(parser);
  if (match(parser, TOKEN_ELSE)) statement(parser);
  patchJump(parser, elseJump);
}

static void whileStatement(Parser *parser) {
  Token *whileToken = next(parser);
  int loopStart = currentChunk(parser)->byteCode.count;
  expression(parser);

  int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
  emitOp(parser, OP_POP);
  statement(parser);
  emitLoop(parser, loopStart, whileToken);

  patchJump(parser, exitJump);
  emitOp(parser, OP_POP);
}

static void expressionStatement(Parser *parser) {
  expression(parser);
  emitOp(parser, OP_POP);
}

static void synchronize(Parser *parser) {
  parser->hadError = false;
  while (peek(parser)->type != TOKEN_EOF) {
    if (match(parser, TOKEN_POINT_COMMA) || peek(parser)->type == TOKEN_NEWLINE)
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
  skipWhiteSpace(parser);
  Token *current = peek(parser);

  switch (current->type) {
  case TOKEN_PRINT:                 printStatement(parser); break;
  case TOKEN_IF:                    ifStatement(parser); break;
  case TOKEN_WHILE:                 whileStatement(parser); break;
  case TOKEN_LEFT_SQUIGGLY_BRACKET: {
    beginScope(parser);
    block(parser);
    endScope(parser);
    break;
  }
  case TOKEN_POINT_COMMA:
    // Report error, logic underneath will deal with cleanup
    errorFromToken(current, "Expected statement, but got ';'.");
    break;
  default: expressionStatement(parser); break;
  }

  // Possibly skip ';'
  match(parser, TOKEN_POINT_COMMA);
}

static void declaration(Parser *parser) {
  statement(parser);
  if (parser->hadError) synchronize(parser);
}

static void program(Parser *parser) {
  while (peek(parser)->type != TOKEN_EOF) {
    if (peek(parser)->type == TOKEN_EOF) break;
    declaration(parser);
  }
  emitOp(parser, OP_RETURN);
}

void parse(Parser *parser) { program(parser); }

#endif