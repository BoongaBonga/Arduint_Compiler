#ifndef ARDUINT_PARSER_C
#define ARDUINT_PARSER_C

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "cmdLineFlags.h"
#include "collection.h"
#include "debug.h"
#include "errorHandling.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "value.h"
#include "vm.h"

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

static void initCompiler(Parser *parser, FunctionType type) {
  parser->compiler.function = NULL;
  parser->compiler.type = type;
  parser->compiler.localCount = 0;
  parser->compiler.scopeDepth = 0;
  parser->compiler.function = newFunction(&parser->objects);

  Local *local = &parser->compiler.locals[parser->compiler.localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
}

Parser initParser(Vector *tokens) {
  Parser parser;
  parser.isValidProgram = true;
  parser.currentIndex = 0;
  parser.tokens = tokens;

  parser.hadError = false;
  parser.isInLoop = false;

  parser.stringPool = initTable();
  parser.globalStrings = initTable();
  parser.objects = NULL;
  initCompiler(&parser, TYPE_SCRIPT);

  parser.breaksToPatch = initVector(sizeof(Vector));
  parser.continueIndices = initVector(sizeof(int));
  return parser;
}

void freeParser(Parser *parser) {
  freeTable(&parser->stringPool);
  freeTable(&parser->globalStrings);

  for (int i = 0; i < parser->breaksToPatch.count; i++) {
    Vector *vec = (Vector *)vecGet(&parser->breaksToPatch, i);
    freeVector(vec);
  }
  freeVector(&parser->breaksToPatch);
  freeVector(&parser->continueIndices);
}

Chunk *currentChunk(Parser *parser) { return parser->compiler.function->chunk; }

static Token *peek(Parser *parser) {
  return vecGet(parser->tokens, parser->currentIndex);
}

static Token *previous(Parser *parser) {
  if (parser->currentIndex == 0) return NULL;
  return vecGet(parser->tokens, parser->currentIndex - 1);
}

static Token *previousNonNewline(Parser *parser) {
  for (int i = parser->currentIndex - 1; i >= 0; i--) {
    Token *token = vecGet(parser->tokens, i);
    if (token->type != TOKEN_NEWLINE) return token;
  }
  return peek(parser);
}

static Token *peekBehind(Parser *parser, int offset) {
  if (parser->currentIndex - offset < 0) return NULL;
  return vecGet(parser->tokens, parser->currentIndex - offset);
}
static Token *peekAhead(Parser *parser, int offset) {
  if (parser->currentIndex + offset > currentChunk(parser)->byteCode.count)
    return NULL;
  Token *tok = vecGet(parser->tokens, parser->currentIndex - offset);
  printf("\n");
  printToken(tok);
  printf(": hi\n");
  return tok;
}

static Token *next(Parser *parser) {
  if (peek(parser)->type == TOKEN_EOF) return peek(parser);
  return vecGet(parser->tokens, parser->currentIndex++);
}

static void skipWhiteSpace(Parser *parser) {
  while (peek(parser)->type == TOKEN_NEWLINE) next(parser);
}

static void parserErrorState(Parser *parser) {
  parser->isValidProgram = false;
  parser->hadError = true;
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

  parserErrorState(parser);
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
  pushLine(currentChunk(parser), previousNonNewline(parser)->line);
}
static void emitOps(Parser *parser, OpCode op1, OpCode op2) {
  writeOperation(currentChunk(parser), op1);
  writeOperation(currentChunk(parser), op2);
  int line = previousNonNewline(parser)->line;
  pushLine(currentChunk(parser), line);
  pushLine(currentChunk(parser), line);
}

static void emitLoop(Parser *parser, int loopStart, Token *loopToken) {
  emitOp(parser, OP_LOOP);

  int offset = currentChunk(parser)->byteCode.count - loopStart + 2;
  if (offset > UINT16_MAX) {
    errorFromToken(loopToken, "Loop body too large.");
    parserErrorState(parser);
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
    parserErrorState(parser);
  }

  *(uint8_t *)vecGet(&currentChunk(parser)->byteCode, offset) =
      (jump >> 8) & 0xff;
  *(uint8_t *)vecGet(&currentChunk(parser)->byteCode, offset + 1) = jump & 0xff;
}

static void errorUnexpectedToken(Parser *parser, Token *token) {
  // The error already got reported by the scanner
  if (token->type == TOKEN_ERROR) return;
  char *src = copySourceStr(token->start, token->length);
  errorFromToken(token, "Unexpected token '%s'", src);
  parserErrorState(parser);
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

const OpCode tokenOps[79][3] = {
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

    [TOKEN_PLUS_PLUS] = {OP_ADD, OP_ERROR, OP_ADD},
    [TOKEN_MINUS_MINUS] = {OP_SUBTRACT, OP_ERROR, OP_SUBTRACT},

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

static void enterLoop(Parser *parser, int continueIndex) {
  Vector breakVec = initVector(sizeof(int));
  vecPush(&parser->breaksToPatch, &breakVec);
  vecPush(&parser->continueIndices, &continueIndex);
}

static void exitLoop(Parser *parser, int breakIndex) {
  Vector breakVec;
  vecPop(&parser->breaksToPatch, &breakVec);
  vecPop(&parser->continueIndices, NULL);

  for (int i = 0; i < breakVec.count; i++) {
    // Pop an offset, and at that offset we replace the jump target
    int index;
    vecPop(&breakVec, &index);

    // Calculate offset (We can safely assume jump < u16MAX since loops can't be
    // that big)
    uint16_t offset = breakIndex - index - 2;

    uint8_t *breakStmt =
        (uint8_t *)vecGet(&currentChunk(parser)->byteCode, index);

    *(breakStmt + 1) = (uint8_t)(offset >> 8);
    *(breakStmt + 2) = (uint8_t)offset;
  }

  freeVector(&breakVec);
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
  ObjString *identifierName = copyStringVal(&parser->objects, name->start,
                                            name->length, &parser->stringPool);
  return tableGet(&parser->globalStrings, identifierName, NULL);
}

static uint16_t identifierConstant(Parser *parser, Token *name) {
  // Check whether name is already in globals
  ObjString *identifierName = copyStringVal(&parser->objects, name->start,
                                            name->length, &parser->stringPool);

  Value value;
  bool containsName = tableGet(&parser->globalStrings, identifierName, &value);
  // if it is in globals, return that index
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
    parserErrorState(parser);
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
      parserErrorState(parser);
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
      if (peek(parser)->type != TOKEN_EQUALS) {
        errorFromToken(name, "Cannot read from an undeclared variable.");
        parserErrorState(parser);
      }
      setOperation = OP_SET_LOCAL;
      arg = parseVariable(parser);
    }
  }

  if (canAssign && match(parser, TOKEN_EQUALS)) {
    expression(parser);
    emitOps(parser, setOperation, (uint8_t)arg);
  } else if (canAssign && isAssignmentToken(peek(parser))) {
    TokenType op = next(parser)->type;

    // Get var -> add expr -> set global
    emitOps(parser, getOperation, (uint8_t)arg);
    expression(parser);

    emitOp(parser, getOp(op, INFIX));
    emitOps(parser, setOperation, (uint8_t)arg);
  } else if (peekBehind(parser, 2)->type == TOKEN_PLUS_PLUS ||
             peekBehind(parser, 2)->type == TOKEN_MINUS_MINUS) {
    // Get var -> add 1 -> set var
    TokenType op = peekBehind(parser, 2)->type;
    emitOps(parser, getOperation, (uint8_t)arg);
    emitOps(parser, OP_PUSH_1, getOp(op, PREFIX));
    emitOps(parser, setOperation, (uint8_t)arg);
  } else if (peek(parser)->type == TOKEN_PLUS_PLUS ||
             peek(parser)->type == TOKEN_MINUS_MINUS) {
    // Get var -> add 1 -> set var
    TokenType op = peek(parser)->type;
    emitOps(parser, getOperation, (uint8_t)arg);
    emitOps(parser, getOperation, (uint8_t)arg);
    emitOps(parser, OP_PUSH_1, getOp(op, PREFIX));
    emitOps(parser, setOperation, (uint8_t)arg);
    emitOp(parser, OP_POP);
  } else {
    emitOps(parser, getOperation, (uint8_t)arg);
  }
}

static void identifier(Parser *parser, Token *name, bool canAssign) {
  if (name->type != TOKEN_IDENTIFIER) {
    errorFromToken(name, "Expect variable.");
    parserErrorState(parser);
    return;
  }
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
    value = OBJECT_VAL(copyStringVal(&parser->objects, val->start + 1,
                                     val->length - 2, &parser->stringPool));
    break;
  }
  default: {
    char *source = copySourceStr(val->start, val->length);
    errorFromToken(val, "Cannot parse a value from '%s'", source);
    free(source);
    parserErrorState(parser);
    return;
  }
  }

  // If the constant offset is bigger than 255, switch to 2-byte adresses
  if (currentChunk(parser)->constants.count < 256) {
    emitOps(parser, OP_CONSTANT_8, currentChunk(parser)->constants.count);
  } else if (currentChunk(parser)->constants.count < UINT16_MAX) {
    emitOp(parser, OP_CONSTANT_16);
    uint16_t index = currentChunk(parser)->constants.count;
    emitOps(parser, (uint8_t)(index >> 8), (uint8_t)index);
  } else {
    errorFromToken(val, "Too many constants.");
    parserErrorState(parser);
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
  case TOKEN_PLUS_PLUS:
  case TOKEN_MINUS_MINUS:        return PREC_POSTFIX * 2;
  default:                       return -1;
  }
}

static int postfixBindingPower(TokenType op) {
  switch (op) {
  case TOKEN_PLUS_PLUS:
  case TOKEN_MINUS_MINUS: return PREC_POSTFIX * 2 + 1;
  default:                return -1;
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
    prefix(parser, next(parser), rightBindingPower);
    return 0;
  }
  case TOKEN_PLUS_PLUS:
  case TOKEN_MINUS_MINUS: {
    // try parsing a variable
    identifier(parser, next(parser), false);
    return 0;
  }
  case TOKEN_MINUS:
  case TOKEN_NOT:   {
    int rightBindingPower = prefixBindingPower(current->type);
    prefix(parser, next(parser), rightBindingPower);

    emitOp(parser, getOp(current->type, PREFIX));
    return 0;
  }

  // Brackets
  case TOKEN_LEFT_ROUND_BRACKET: {
    expressionBp(parser, PREC_EXPRESSION);
    consume(parser, TOKEN_RIGHT_ROUND_BRACKET, current,
            "Found no closing parentheses after '('.");
    return 0;
  }

  default: {
    // Report error and try parsing the next token
    errorUnexpectedToken(parser, current);
    parserErrorState(parser);
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
    emitOps(parser, OP_CONVERT_TYPE, CONV_BOOL);
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
    emitOps(parser, OP_CONVERT_TYPE, CONV_BOOL);
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

static int postfix(Parser *parser, Token *current, int minBp) {
  int lBindingPower = postfixBindingPower(current->type);
  if (lBindingPower < minBp) return 1;

  switch (current->type) {
  case TOKEN_PLUS_PLUS:
  case TOKEN_MINUS_MINUS: {
    next(parser);
    return 0;
  }
  default: return 0;
  }
}

static void expressionBp(Parser *parser, int minBp) {
  skipWhiteSpace(parser);
  Token *current = next(parser);

  if (current->type == TOKEN_EOF) return;

  if (-1 == prefix(parser, current, minBp)) return;

  for (;;) {
    skipWhiteSpace(parser);
    Token *current = peek(parser);
    if (current->type == TOKEN_EOF) break;
    if (postfixBindingPower(current->type) != -1) {
      if (postfix(parser, current, minBp) == 1) break;
      continue;
    }

    if (infix(parser, current, minBp) == 1) break;
  }

  // Catch equality silently returning
  bool canAssign = minBp <= PREC_ASSIGN;
  if (!canAssign && isAssignmentToken(peek(parser))) {
    // Reuse the current token at the start of the expression
    errorFromToken(peek(parser), "Invalid assignment target.");
    parserErrorState(parser);
    next(parser);
  }
  return;
}

static void expression(Parser *parser) { expressionBp(parser, 0); }

//____________________________________________
//                STATEMENTS
//____________________________________________

static void expressionStatement(Parser *parser) {
  int localCount = parser->compiler.localCount;
  expression(parser);
  if (parser->compiler.localCount == localCount) emitOp(parser, OP_POP);
}

static void block(Parser *parser) {
  consume(parser, TOKEN_LEFT_SQUIGGLY_BRACKET, peek(parser),
          "Cannot start a block without a '{'.");

  for (;;) {
    skipWhiteSpace(parser);
    if (check(parser, TOKEN_RIGHT_SQUIGGLY_BRACKET) || check(parser, TOKEN_EOF))
      break;

    declaration(parser);
  }

  consume(parser, TOKEN_RIGHT_SQUIGGLY_BRACKET, peek(parser),
          "Excpect '}' after a block.");
}

static void setPinStatement(Parser *parser) {
  next(parser);
  skipWhiteSpace(parser);

  // Parse expression for pin (digital for now)
  expression(parser);

  consume(parser, TOKEN_TO, peek(parser), "Expect 'to' after pin number.");

  // Get value to set it to
  expression(parser);

  emitOp(parser, OP_SETPIN);
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

// This can be optimised WAY more if wanted
static void switchStatement(Parser *parser) {
  next(parser);

  skipWhiteSpace(parser);
  expression(parser);

  skipWhiteSpace(parser);
  if (!consume(parser, TOKEN_LEFT_SQUIGGLY_BRACKET, peek(parser),
               "Expect '{' after switch operand."))
    return;

  // Consume cases and emit if / else statements
  Vector exitJumps = initVector(sizeof(int));
  do {
    skipWhiteSpace(parser);
    if (peek(parser)->type == TOKEN_DEFAULT) break;

    if (!consume(parser, TOKEN_CASE, peek(parser),
                 "Expect 'case' inside switch statement."))
      return;

    // The value to match to gets compared to the switch value
    expression(parser);
    emitOp(parser, OP_CASE);
    // If this is not it, jump to the next case.
    int elseJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitOp(parser, OP_POP);

    skipWhiteSpace(parser);
    if (!consume(parser, TOKEN_COLON, peek(parser),
                 "Expect ':' after switch case."))
      return;

    statement(parser);

    // Register exitJump offset to be patched
    int exitJump = emitJump(parser, OP_JUMP);
    vecPush(&exitJumps, &exitJump);

    patchJump(parser, elseJump);
    emitOp(parser, OP_POP);

    skipWhiteSpace(parser);
    if (check(parser, TOKEN_EOF)) {
      errorFromToken(previous(parser), "Expect '}' after switch statement.");
      parserErrorState(parser);
      return;
    }
  } while (!match(parser, TOKEN_RIGHT_SQUIGGLY_BRACKET));

  // Potentially parse a default statement
  if (peek(parser)->type == TOKEN_DEFAULT) {
    next(parser);
    if (!consume(parser, TOKEN_COLON, peek(parser),
                 "Expect ':' after default."))
      return;

    // Parse default statement
    statement(parser);

    skipWhiteSpace(parser);
    if (!consume(parser, TOKEN_RIGHT_SQUIGGLY_BRACKET, peek(parser),
                 "Expect end of switch statement after default case."))
      return;
  }

  // Patch exit jumps before default case, so they automatically go to default
  for (int i = exitJumps.count; i > 0; i--) {
    int exitJump;
    vecPop(&exitJumps, &exitJump);
    patchJump(parser, exitJump);
  }

  // Pop switch value
  emitOp(parser, OP_POP);
}

static void loopStatement(Parser *parser) {
  Token *loopToken = next(parser);
  int loopStart = currentChunk(parser)->byteCode.count;

  enterLoop(parser, loopStart);

  statement(parser);
  emitLoop(parser, loopStart, loopToken);

  exitLoop(parser, currentChunk(parser)->byteCode.count - 1);
}

static void whileStatement(Parser *parser) {
  Token *whileToken = next(parser);
  int loopStart = currentChunk(parser)->byteCode.count;

  enterLoop(parser, loopStart);

  expression(parser);

  int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
  emitOp(parser, OP_POP);
  statement(parser);
  emitLoop(parser, loopStart, whileToken);

  patchJump(parser, exitJump);
  emitOp(parser, OP_POP);

  exitLoop(parser, currentChunk(parser)->byteCode.count - 1);
}

static void doWhileStatement(Parser *parser) {
  // Do
  Token *doToken = next(parser);
  int loopStart = currentChunk(parser)->byteCode.count;

  enterLoop(parser, loopStart);

  // Something
  statement(parser);

  // while
  skipWhiteSpace(parser);
  consume(parser, TOKEN_WHILE, peek(parser),
          "Expect a 'while [condition]' after statement.");

  // Condition
  expression(parser);
  int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
  emitOp(parser, OP_POP);
  emitLoop(parser, loopStart, doToken);

  patchJump(parser, exitJump);
  emitOp(parser, OP_POP);

  exitLoop(parser, currentChunk(parser)->byteCode.count - 1);
}

static void forStatement(Parser *parser) {
  beginScope(parser);

  Token *forToken = next(parser);
  skipWhiteSpace(parser);
  bool hasParentheses = match(parser, TOKEN_LEFT_ROUND_BRACKET);

  // Initialiser // if (<-here
  skipWhiteSpace(parser);
  if (match(parser, TOKEN_POINT_COMMA)) {
    // No initialiser
  } else {
    expressionStatement(parser);

    skipWhiteSpace(parser);
    consume(parser, TOKEN_POINT_COMMA, peek(parser),
            "Expect a ';' after initialiser.");
  }

  skipWhiteSpace(parser);

  // Loop condition // if ( a = 0 ; <-here
  int loopStart = currentChunk(parser)->byteCode.count;

  int exitJump = -1;
  if (!match(parser, TOKEN_POINT_COMMA)) {
    expression(parser);

    skipWhiteSpace(parser);
    consume(parser, TOKEN_POINT_COMMA, peek(parser),
            "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitOp(parser, OP_POP); // Condition.
  }

  // Increment // if ( a = 0; a < 10 ; <-here
  skipWhiteSpace(parser);
  if (!match(parser, TOKEN_POINT_COMMA)) {
    int bodyJump = emitJump(parser, OP_JUMP);
    int incrementStart = currentChunk(parser)->byteCode.count;

    expression(parser);
    emitOp(parser, OP_POP);

    skipWhiteSpace(parser);
    consume(parser, TOKEN_RIGHT_ROUND_BRACKET, peek(parser),
            "Expect ')' after for clauses.");

    emitLoop(parser, loopStart, forToken);
    loopStart = incrementStart;
    patchJump(parser, bodyJump);
  }

  enterLoop(parser, loopStart);

  statement(parser);

  emitLoop(parser, loopStart, forToken);

  if (exitJump != -1) {
    patchJump(parser, exitJump);
    emitOp(parser, OP_POP); // Condition
  }

  exitLoop(parser, currentChunk(parser)->byteCode.count - 1);

  endScope(parser);
}

static void breakStatement(Parser *parser) {
  if (parser->breaksToPatch.count <= 0) {
    errorFromToken(next(parser), "Cannot break out of program.");
    parserErrorState(parser);
    return;
  }
  next(parser);

  // push a jump instruction to the breaksToPatch vector
  // for any loop to deal with
  int offset = currentChunk(parser)->byteCode.count;

  Vector *breakOffsets =
      vecGet(&parser->breaksToPatch, parser->breaksToPatch.count - 1);
  vecPush(breakOffsets, &offset);

  // push the jump instruction with dummy values
  emitOp(parser, OP_JUMP);
  emitOps(parser, 0xff, 0xff);
}

static void continueStatement(Parser *parser) {
  if (parser->continueIndices.count <= 0) {
    errorFromToken(next(parser), "Cannot continue inside of program.");
    parserErrorState(parser);
    return;
  }
  next(parser);

  emitOp(parser, OP_LOOP);

  int loopStart = *(int *)vecGet(&parser->continueIndices,
                                 parser->continueIndices.count - 1);

  int offset = currentChunk(parser)->byteCode.count - loopStart + 2;

  emitOps(parser, (uint8_t)(offset >> 8), (uint8_t)offset);
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
    case TOKEN_DO:
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
    case TOKEN_SWITCH:
    case TOKEN_LOOP:
    case TOKEN_RETURN:
    case TOKEN_SETPIN:
    case TOKEN_READPIN:
    case TOKEN_DELAY:
    case TOKEN_LEFT_SQUIGGLY_BRACKET:
    case TOKEN_DELAY_MICROS:          return; // Found new statement start
    default:                          next(parser); continue;
    }
  }
  skipWhiteSpace(parser);
}

static void statement(Parser *parser) {
  skipWhiteSpace(parser);
  Token *current = peek(parser);

  switch (current->type) {
  case TOKEN_SETPIN:                setPinStatement(parser); break;
  case TOKEN_PRINT:                 printStatement(parser); break;
  case TOKEN_IF:                    ifStatement(parser); break;
  case TOKEN_SWITCH:                switchStatement(parser); break;
  case TOKEN_LOOP:                  loopStatement(parser); break;
  case TOKEN_WHILE:                 whileStatement(parser); break;
  case TOKEN_DO:                    doWhileStatement(parser); break;
  case TOKEN_FOR:                   forStatement(parser); break;
  case TOKEN_BREAK:                 breakStatement(parser); break;
  case TOKEN_CONTINUE:              continueStatement(parser); break;
  case TOKEN_LEFT_SQUIGGLY_BRACKET: {
    beginScope(parser);
    block(parser);
    endScope(parser);
    break;
  }
  case TOKEN_POINT_COMMA:
    // Report error, logic underneath will deal with cleanup
    errorFromToken(current, "Expected statement, but got ';'.");
    parserErrorState(parser);
    break;
  default: expressionStatement(parser); break;
  }

  // Possibly skip ';'
  match(parser, TOKEN_POINT_COMMA);
}

static void declaration(Parser *parser) {
  statement(parser);
  if (parser->hadError) {
    synchronize(parser);
  }
}

static void program(Parser *parser) {
  while (peek(parser)->type != TOKEN_EOF) {
    if (peek(parser)->type == TOKEN_EOF) break;
    declaration(parser);
  }
  emitOp(parser, OP_RETURN);
}

ObjFunction *parse(Parser *parser) {
  program(parser);

  ObjFunction *function = parser->compiler.function;

  if (cmdFlags.debugParser && parser->isValidProgram) {
    printChunk(currentChunk(parser),
               function->name != NULL ? function->name->chars : "<script>");
  }

  return function;
}

#endif