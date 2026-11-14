#ifndef ARDUINT_VM_H
#define ARDUINT_VM_H

#include "collection.h"
#include "object.h"
#include "parser.h"

#define STACK_MAX 256

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
  Chunk *chunk;
  int ip;

  Value stack[STACK_MAX];
  Value *stackTop;

  Table globals;
  Value locals[UINT8_COUNT];
  int localCount;

  Object *objects;

  Table strings;
} VM;

void initVM(Parser *parser, VM *vm);
void freeVM(VM *vm);

InterpretResult interpret(VM *vm, Chunk *chunk);
void push(VM *vm, Value value);
Value pop(VM *vm);

#endif