#ifndef ARDUINT_DEBUG_C
#define ARDUINT_DEBUG_C

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "collection.h"
#include "errorHandling.h"
#include "parser.h"
#include "scanner.h"
#include "token.h"

const char *lexemes[71] = {
    "IDENTIFIER",
    "NUMBER",
    "STRING",
    "NULL",

    // Control flow
    "IF",
    "THEN",
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
    "XOR",

    // One-character tokens
    "BACKSLASH",
    "COMMA",
    "POINT_COMMA",
    "COLON",
    "DOT",
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

    "EQUALS",
    "MINUS_EQUALS",
    "PLUS_EQUALS",
    "SLASH_EQUALS",
    "STAR_EQUALS",
    "POWER_EQUALS",
    "AND_EQUALS",
    "OR_EQUALS",
    "PERCENT_EQUALS",

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

void printLexeme(Token *token) {
  char *str = copySourceStr(token->start, token->length);
  printf("%s", str);
  free(str);
}

void printObject(Object *object) {
  switch (object->type) {
  case OBJ_STRING: printf("\"%s\"", ((ObjString *)object)->chars);
  }
}

void printValue(Value *value) {
  switch (value->type) {
  case VAL_NULL:   printf("NULL"); break;
  case VAL_NUMBER: printf("%lf", value->as.number); break;
  case VAL_OBJECT: printObject(value->as.object); break;
  case VAL_BOOL:   printf("%d", value->as.boolean); break;
  default:         printf("ErrorVal"); break;
  }
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constantInstruction(const char *name, bool is16b, Chunk *chunk,
                               int offset) {
  int constant;
  if (is16b) {
    uint8_t constantL = *(uint8_t *)vecGet(&chunk->byteCode, offset + 1);
    uint8_t constantH = *(uint8_t *)vecGet(&chunk->byteCode, offset + 2);
    constant = ((uint16_t)constantL << 8) | ((uint16_t)constantH);
  } else {
    constant = *(uint8_t *)vecGet(&chunk->byteCode, offset + 1);
  }

  printf("%-16s %4d '", name, constant);
  Value *value = (Value *)vecGet(&chunk->constants, constant);
  printValue(value);
  printf("'\n");
  return offset + 2 + is16b;
}

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t slot = *(uint8_t *)vecGet(&chunk->byteCode, offset + 1);
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk,
                           int offset) {
  uint16_t jump =
      (uint16_t)(*(uint8_t *)vecGet(&chunk->byteCode, offset + 1) << 8);
  jump |= *(uint8_t *)vecGet(&chunk->byteCode, offset + 2);
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

static int printInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);

  uint8_t instruction = *(uint8_t *)vecGet(&chunk->byteCode, offset);

  switch (instruction) {
  case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
  case OP_ERROR:  return simpleInstruction("ERROR_OP", offset);
  case OP_NO_OP:  return simpleInstruction("OP_NO_OP", offset);
  case OP_CONSTANT_8:
    return constantInstruction("OP_CONSTANT_8", false, chunk, offset);
  case OP_CONSTANT_16:
    return constantInstruction("OP_CONSTANT_16", true, chunk, offset);
  case OP_DEFINE_GLOBAL:
    return constantInstruction("OP_DEFINE_GLOBAL", false, chunk, offset);
  case OP_GET_GLOBAL:
    return constantInstruction("OP_GET_GLOBAL", false, chunk, offset);
  case OP_SET_GLOBAL:
    return constantInstruction("OP_SET_GLOBAL", false, chunk, offset);
  case OP_GET_LOCAL: return byteInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_SET_LOCAL: return byteInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_PRINT:     return simpleInstruction("OP_PRINT", offset);

  case OP_JUMP:      return jumpInstruction("OP_JUMP", 1, chunk, offset);
  case OP_JUMP_IF_FALSE:
    return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);

  case OP_ASSIGN:     return simpleInstruction("OP_ASSIGN", offset);
  case OP_POP:        return simpleInstruction("OP_POP", offset);
  case OP_ADD:        return simpleInstruction("OP_ADD", offset);
  case OP_SUBTRACT:   return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:   return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:     return simpleInstruction("OP_DIVIDE", offset);
  case OP_MODULO:     return simpleInstruction("OP_MODULO", offset);
  case OP_POWER:      return simpleInstruction("OP_POWER", offset);
  case OP_NEGATE:     return simpleInstruction("OP_NEGATE", offset);
  case OP_NOT:        return simpleInstruction("OP_NOT", offset);
  case OP_AND:        return simpleInstruction("OP_AND", offset);
  case OP_OR:         return simpleInstruction("OP_OR", offset);
  case OP_XOR:        return simpleInstruction("OP_XOR", offset);

  case OP_EQUALS:     return simpleInstruction("OP_EQUALS", offset);
  case OP_NOT_EQUALS: return simpleInstruction("OP_NOT_EQUALS", offset);
  case OP_GREATER:    return simpleInstruction("OP_GREATER", offset);
  case OP_GREATER_OR_EQUAL:
    return simpleInstruction("OP_GREATER_OR_EQUAL", offset);
  case OP_LESS:          return simpleInstruction("OP_LESS", offset);
  case OP_LESS_OR_EQUAL: return simpleInstruction("OP_LESS_OR_EQUAL", offset);
  case OP_AND_AND:       return simpleInstruction("OP_AND_AND", offset);
  case OP_OR_OR:         return simpleInstruction("OP_OR_OR", offset);
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