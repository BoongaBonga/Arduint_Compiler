#ifndef ARDUINT_VM_C
#define ARDUINT_VM_C

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cmdLineFlags.h"
#include "collection.h"
#include "debug.h"
#include "errorHandling.h"
#include "object.h"
#include "parser.h"
#include "value.h"
#include "vm.h"

typedef enum { EQ_NOT_EQUAL, EQ_EQUAL, EQ_ERROR } Equality;

static void resetStack(VM *vm) { vm->stackTop = vm->stack; }

void initVM(Parser *parser, VM *vm) {
  vm->objects = NULL;
  vm->strings = parser->stringPool;
  vm->globals = parser->globalStrings;
  vm->localCount = 0;
  resetStack(vm);
}

void freeVM(VM *vm) { freeObjects(vm->objects); }

void push(VM *vm, Value value) { *vm->stackTop++ = value; }
Value pop(VM *vm) { return *(--vm->stackTop); }
Value peek(VM *vm, int offset) { return *(vm->stackTop - 1 - offset); }

static int isTruthy(Value value) {
  switch (value.type) {
  case VAL_BOOL:   return value.as.boolean;
  case VAL_NUMBER: return value.as.number != 0.0f;
  case VAL_NULL:   return false;
  case VAL_OBJECT: {
    switch (value.as.object->type) {
    case OBJ_STRING: return AS_OBJ_STRING(value)->length == 1;
    }
  }
  }
}

static Equality isEqual(Value a, Value b) {
  if (a.type != b.type) {
    runtimeError("Cannot compare two values of different types for equality.");
    return EQ_ERROR;
  }
  switch (a.type) {
  case VAL_BOOL: return a.as.boolean == b.as.boolean ? EQ_EQUAL : EQ_NOT_EQUAL;
  case VAL_NUMBER: return a.as.number == b.as.number ? EQ_EQUAL : EQ_NOT_EQUAL;
  case VAL_OBJECT: {
    if (!IS_STRING(a) || !IS_STRING(b)) {
      runtimeError("Can only compare two string objects with eachother.");
      return EQ_ERROR;
    }
    ObjString *bStr = AS_OBJ_STRING(b);
    ObjString *aStr = AS_OBJ_STRING(a);
    return (aStr->hash != bStr->hash || aStr->length != bStr->length ||
            0 == memcmp(aStr->chars, bStr->chars, aStr->length))
               ? EQ_EQUAL
               : EQ_NOT_EQUAL;
  }
  }
}

static void concatenate(VM *vm) {
  ObjString *b = AS_OBJ_STRING(pop(vm));
  ObjString *a = AS_OBJ_STRING(pop(vm));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(&vm->objects, chars, length, &vm->strings);
  push(vm, OBJECT_VAL(result));
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*(uint8_t *)vecGet(&vm->chunk->byteCode, vm->ip++))
#define READ_SHORT() ((uint16_t)READ_BYTE() << 8 | READ_BYTE())
#define READ_CONSTANT_8() (*(Value *)vecGet(&vm->chunk->constants, READ_BYTE()))
#define READ_CONSTANT_16()                                                     \
  (*(Value *)vecGet(&vm->chunk->constants, READ_SHORT()))
#define CHECK_NUMBERS(errorMsg)                                                \
  if (peek(vm, 0).type != VAL_NUMBER || peek(vm, 1).type != VAL_NUMBER) {      \
    runtimeError(errorMsg);                                                    \
    return INTERPRET_RUNTIME_ERROR;                                            \
  }
#define CHECK_BOOLEANS(errorMsg)                                               \
  if (peek(vm, 0).type != VAL_BOOL || peek(vm, 1).type != VAL_BOOL) {          \
    runtimeError(errorMsg);                                                    \
    return INTERPRET_RUNTIME_ERROR;                                            \
  }
#define CHECK_NUMBER_OR_BOOLEAN(errorMsg)                                      \
  if ((peek(vm, 0).type != VAL_NUMBER && peek(vm, 1).type != VAL_BOOL) ||      \
      (peek(vm, 0).type != peek(vm, 1).type)) {                                \
    runtimeError(errorMsg);                                                    \
    return INTERPRET_RUNTIME_ERROR;                                            \
  }
#define BINARY_OP(op, resultMacro, errorMsg)                                   \
  do {                                                                         \
    CHECK_NUMBERS(errorMsg)                                                    \
    double b = pop(vm).as.number;                                              \
    double a = pop(vm).as.number;                                              \
    push(vm, resultMacro(a op b));                                             \
  } while (false)
#define UNARY_OP(op, errorMsg)                                                 \
  do {                                                                         \
    if (peek(vm, 0).type != VAL_NUMBER) {                                      \
      runtimeError(errorMsg);                                                  \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    (*(vm->stackTop - 1)).as.number = op(*(vm->stackTop - 1)).as.number;       \
  } while (false)
#define INT_BINARY_OP(op, errorMsg)                                            \
  do {                                                                         \
    CHECK_NUMBER_OR_BOOLEAN(errorMsg)                                          \
    if (peek(vm, 0).type == VAL_NUMBER) {                                      \
      long b = (long)pop(vm).as.number;                                        \
      long a = (long)pop(vm).as.number;                                        \
      push(vm, NUMBER_VAL((double)(a op b)));                                  \
    } else {                                                                   \
      bool b = pop(vm).as.boolean;                                             \
      bool a = pop(vm).as.boolean;                                             \
      push(vm, BOOL_VAL(a op b));                                              \
    }                                                                          \
  } while (false)

  if (cmdFlags.debugVM) {
    printf("\n== Execution ==\n");
  }

  for (;;) {
    if (cmdFlags.debugVM) {
      printf("          ");
      for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
        printf("[ ");
        printValue(slot);
        printf(" ]");
      }
      printf("\n");
      printInstruction(vm->chunk, vm->ip);
    }

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_RETURN:     return INTERPRET_OK;
    case OP_NO_OP:      break;
    case OP_CONSTANT_8: {
      Value constant = READ_CONSTANT_8();
      push(vm, constant);
      break;
    }
    case OP_CONSTANT_16: {
      Value constant = READ_CONSTANT_16();
      push(vm, constant);
      break;
    }
    case OP_GET_GLOBAL: {
      Value identifierName = READ_CONSTANT_8();
      Value val;
      tableGet(&vm->globals, AS_OBJ_STRING(identifierName), &val);
      push(vm, val);
      break;
    }
    case OP_SET_GLOBAL: {
      Value identifierName = READ_CONSTANT_8();
      tableSet(&vm->globals, AS_OBJ_STRING(identifierName), peek(vm, 0));
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(vm, vm->stack[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      vm->stack[slot] = peek(vm, 0);
      break;
    }
    case OP_PRINT: {
      Value val = pop(vm);
      printValue(&val);
      printf("\n");
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (!isTruthy(peek(vm, 0))) vm->ip += offset;
      break;
    }
    case OP_JUMP: vm->ip += READ_SHORT(); break;
    case OP_LOOP: vm->ip -= READ_SHORT(); break;
    case OP_POP:  pop(vm); break;

    case OP_ADD:  {
      if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
        concatenate(vm);
      } else if (peek(vm, 0).type == VAL_NUMBER &&
                 peek(vm, 1).type == VAL_NUMBER) {
        double b = pop(vm).as.number;
        double a = pop(vm).as.number;
        push(vm, NUMBER_VAL(a + b));
      } else {
        runtimeError("Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SUBTRACT:
      BINARY_OP(-, NUMBER_VAL, "Can only subtract two numbers.");
      break;
    case OP_MULTIPLY:
      BINARY_OP(*, NUMBER_VAL, "Can only multiply two numbers.");
      break;
    case OP_DIVIDE: {
      if (peek(vm, 0).type == VAL_NUMBER && peek(vm, 0).as.number == 0.0f) {
        runtimeError("Cannot divide by zero.");
        return INTERPRET_RUNTIME_ERROR;
      }
      BINARY_OP(/, NUMBER_VAL, "Can only divide two numbers.");
      break;
    }
    case OP_MODULO: {
      CHECK_NUMBERS("Can only get the modulo of two numbers.")
      double b = pop(vm).as.number;
      double a = pop(vm).as.number;
      push(vm, NUMBER_VAL(fmod(a, b)));
      break;
    }
    case OP_NEGATE: UNARY_OP(-, "Can only negate numbers."); break;
    case OP_POWER:  {
      CHECK_NUMBERS("Can only raise a number to the power of another number.")
      double b = pop(vm).as.number;
      double a = pop(vm).as.number;
      push(vm, NUMBER_VAL(pow(a, b)));
      break;
    }
    case OP_AND:
      INT_BINARY_OP(&, "Can only bitwise and two numbers or two booleans.");
      break;
    case OP_NOT: {
      if (peek(vm, 0).type != VAL_BOOL) {
        runtimeError("Can only not a boolean.");
        return INTERPRET_RUNTIME_ERROR;
      }
      (*(vm->stackTop - 1)).as.boolean = !(*(vm->stackTop - 1)).as.boolean;
      break;
    }
    case OP_OR:
      INT_BINARY_OP(|, "Can only bitwise or two numbers or two booleans.");
      break;
    case OP_XOR:
      INT_BINARY_OP(^, "Can only bitwise xor two numbers or two booleans.");
      break;

    case OP_EQUALS: {
      Value b = pop(vm);
      Value a = pop(vm);
      Equality equal = isEqual(a, b);
      if (equal == EQ_ERROR) return INTERPRET_RUNTIME_ERROR;
      push(vm, BOOL_VAL(equal));
      break;
    }
    case OP_CASE: {
      Value b = pop(vm);
      Value a = peek(vm, 0);
      Equality equal = isEqual(a, b);
      if (equal == EQ_ERROR) return INTERPRET_RUNTIME_ERROR;
      push(vm, BOOL_VAL(equal));
      break;
    }
    case OP_NOT_EQUALS: {
      Value b = pop(vm);
      Value a = pop(vm);
      Equality equal = isEqual(a, b);
      if (equal == EQ_ERROR) return INTERPRET_RUNTIME_ERROR;
      push(vm, BOOL_VAL(!(bool)equal));
      break;
    }
    case OP_LESS:
      BINARY_OP(<, BOOL_VAL, "Can only compare two numbers.");
      break;
    case OP_LESS_OR_EQUAL:
      BINARY_OP(<=, BOOL_VAL, "Can only compare two numbers.");
      break;
    case OP_GREATER:
      BINARY_OP(>, BOOL_VAL, "Can only compare two numbers.");
      break;
    case OP_GREATER_OR_EQUAL:
      BINARY_OP(>=, BOOL_VAL, "Can only compare two numbers.");
      break;
    case OP_AND_AND: {
      CHECK_BOOLEANS("Can only logic and two booleans")
      bool b = pop(vm).as.boolean;
      bool a = pop(vm).as.boolean;
      push(vm, BOOL_VAL(a && b));
      break;
    }
    case OP_OR_OR: {
      CHECK_BOOLEANS("Can only logic and two booleans")
      bool b = pop(vm).as.boolean;
      bool a = pop(vm).as.boolean;
      push(vm, BOOL_VAL(a || b));
    }

    case OP_ERROR: continue;
    default:       runtimeError("Unknown opcode."); break;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT_8
#undef READ_CONSTANT_16
#undef BINARY_OP
#undef INT_BINARY_OP
#undef CHECK_NUMBERS
#undef CHECK_BOOLEANS
#undef CHECK_NUMBER_OR_BOOLEAN
#undef UNARY_OP
}

InterpretResult interpret(VM *vm, Chunk *chunk) {
  vm->chunk = chunk;
  vm->ip = 0;
  return run(vm);
}

#endif