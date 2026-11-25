#ifndef ARDUINT_VM_H
#define ARDUINT_VM_H

#include <stdint.h>

#include "collection.h"
#include "object.h"
#include "parser.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

#define PIN_COUNT 20
#define LOW 0
#define HIGH 1

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

typedef enum { PINMODE_INPUT, PINMODE_OUTPUT } PinMode;
typedef enum {
  PIN_D0,
  PIN_D1,
  PIN_D2,
  PIN_D3,
  PIN_D4,
  PIN_D5,
  PIN_D6,
  PIN_D7,
  PIN_D8,
  PIN_D9,
  PIN_D10,
  PIN_D11,
  PIN_D12,
  PIN_D13,
  PIN_A0,
  PIN_A1,
  PIN_A2,
  PIN_A3,
  PIN_A4,
  PIN_A5
} PinNumber;

typedef struct {
  uint8_t port;
  uint8_t index;
} PinPosition;

typedef struct VM {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value *stackTop;

  Table globals;
  Value locals[UINT8_COUNT];
  int localCount;

  PinMode pinModes[PIN_COUNT];

  Object *objects;

  Table strings;

  bool needsArduino;
} VM;

void initVM(Parser *parser, VM *vm);
void freeVM(VM *vm);

InterpretResult interpret(VM *vm, ObjFunction *function);
void push(VM *vm, Value value);
Value pop(VM *vm);

#endif