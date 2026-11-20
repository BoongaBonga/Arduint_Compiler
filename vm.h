#ifndef ARDUINT_VM_H
#define ARDUINT_VM_H

#include "collection.h"
#include "object.h"
#include "parser.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef enum {
  CONV_OBJECT,
  CONV_NULL,
  CONV_BOOL,
  CONV_NUMBER,
  CONV_STRING,
  CONV_FUNCTION,
  CONV_ERROR,
} ConversionType;

typedef struct {
  ObjFunction *function;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

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

InterpretResult interpret(VM *vm, ObjFunction *function);
void push(VM *vm, Value value);
Value pop(VM *vm);

#endif