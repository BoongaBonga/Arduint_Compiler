#ifndef ARDUINT_PARSER_H
#define ARDUINT_PARSER_H

#include <stdbool.h>

#include "chunk.h"
#include "collection.h"
#include "common.h"
#include "object.h"
#include "token.h"

typedef struct {
  Token name;
  int depth;
} Local;

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

typedef struct {
  FunctionType type;
  ObjFunction *function;

  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

typedef struct {
  Vector *tokens;
  int currentIndex;

  Vector breaksToPatch;   // jumps that still need to be patched
  Vector continueIndices; // Continue points to patch new continue statements

  bool isValidProgram;
  bool hadError;
  bool isInLoop;
  bool needsArduino;

  Table stringPool;
  Table globalStrings;
  Object *objects;

  Compiler compiler;
} Parser;

Parser initParser(Vector *tokens);
void freeParser(Parser *parser);

ObjFunction *parse(Parser *parser);

#endif