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
typedef enum { CONVRES_OK = 0, CONVRES_ERROR = -1 } ConversionResult;

#define CONVTYPE_COUNT 4 // null, bool, number, string

const ConversionType EQUALITY_PRIORITIES[CONVTYPE_COUNT] = {
    CONV_BOOL, CONV_NUMBER, CONV_STRING, CONV_NULL};
const ConversionType ADDITION_PRIORITIES[CONVTYPE_COUNT] = {
    CONV_STRING, CONV_NUMBER, CONV_BOOL, CONV_NULL};
const ConversionType BINARY_OP_PRIORITIES[CONVTYPE_COUNT] = {
    CONV_NUMBER, CONV_BOOL, CONV_NULL, CONV_NULL};

const char VALUE_TYPE_NAMES[4][8] = {
    [VAL_NULL] = "null",
    [VAL_BOOL] = "bool",
    [VAL_NUMBER] = "number",
    [VAL_OBJECT] = "object",
};

static ValueType convToValueType(ConversionType type) {
  switch (type) {
  case CONV_NULL:     return VAL_NULL;
  case CONV_BOOL:     return VAL_BOOL;
  case CONV_NUMBER:   return VAL_NUMBER;
  case CONV_FUNCTION:
  case CONV_STRING:   return VAL_OBJECT;
  default:            return VAL_NULL;
  }
}
static ConversionType valToConvType(ValueType type) {
  switch (type) {
  case VAL_NULL:   return CONV_NULL;
  case VAL_BOOL:   return CONV_BOOL;
  case VAL_NUMBER: return CONV_NUMBER;
  case VAL_OBJECT: return CONV_OBJECT;
  default:         return CONV_ERROR;
  }
}
static ConversionType objToConvType(ObjType type) {
  switch (type) {
  case OBJ_STRING:   return CONV_STRING;
  case OBJ_FUNCTION: return CONV_FUNCTION;
  default:
    runtimeError("Tried to convert a function to another type.");
    return CONV_ERROR;
  }
}

static void resetStack(VM *vm) {
  vm->stackTop = vm->stack;
  vm->frameCount = 0;
}

void initVM(Parser *parser, VM *vm) {
  vm->objects = NULL;
  vm->strings = initTable();
  tableAddAll(&parser->stringPool, &vm->strings);
  // vm->globals = parser->globalStrings;
  vm->globals = initTable();
  vm->localCount = 0;
  resetStack(vm);
}

void freeVM(VM *vm) {
  freeObjects(vm->objects);
  freeTable(&vm->strings);
  freeTable(&vm->globals);
}

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
    case OBJ_STRING:   return AS_OBJ_STRING(value)->length > 0;
    case OBJ_FUNCTION: return true;
    }
  }
  }
  return false;
}

static bool canConvert(Value value, ConversionType desiredType) {
  if ((desiredType == valToConvType(value.type)) ||
      (valToConvType(value.type) == CONV_OBJECT &&
       objToConvType(value.as.object->type) == desiredType))
    return true;
  if (value.type == VAL_NULL) return desiredType == CONV_STRING;

  switch (value.type) {
  case VAL_BOOL:
    return (desiredType == CONV_NUMBER || desiredType == CONV_STRING);
  case VAL_NUMBER:
    return (desiredType == CONV_BOOL || desiredType == CONV_STRING);
  case VAL_OBJECT: {
    switch (value.as.object->type) {
    case OBJ_FUNCTION: return desiredType == CONV_STRING;
    case OBJ_STRING:   {
      if (desiredType == CONV_NUMBER) {
        char *endPtr;
        ObjString *str = AS_OBJ_STRING(value);
        strtod(str->chars, &endPtr);
        return endPtr == str->chars + str->length;
      }
      return desiredType == CONV_BOOL;
    }
    }
  }
  }
  return false;
}

/// @brief returns VAL_NULL if invalid conversion.
static Value convertValue(VM *vm, Value value, ConversionType desiredType) {
  // Check if input is already at the desired type
  if (desiredType == valToConvType(value.type)) return value;
  if (desiredType == CONV_NULL) return NULL_VAL;
  if (valToConvType(value.type) == CONV_OBJECT) {
    if (objToConvType(value.as.object->type) == desiredType) return value;
  }

  // If not, try converting it.
  switch (value.type) {
  case VAL_NULL: {
    if (desiredType == CONV_STRING)
      return OBJECT_VAL(copyStringVal(&vm->objects, "null", 4, &vm->strings));
    break;
  }
  case VAL_BOOL: {
    bool boolVal = value.as.boolean;
    if (desiredType == CONV_NUMBER) return NUMBER_VAL((double)boolVal);
    if (desiredType == CONV_STRING)
      return OBJECT_VAL(copyStringVal(&vm->objects, boolVal ? "true" : "false",
                                      boolVal ? 4 : 5, &vm->strings));

    break;
  }
  case VAL_NUMBER: {
    double numVal = value.as.number;
    if (desiredType == CONV_BOOL) return BOOL_VAL(isTruthy(value));
    if (desiredType == CONV_STRING) {
      char buffer[50];
      int written = snprintf(buffer, 49, "%.17g", numVal);
      return OBJECT_VAL(
          copyStringVal(&vm->objects, buffer, written, &vm->strings));
    }
    break;
  }
  case VAL_OBJECT: {
    // Try converting function to string
    if (!IS_STRING(value)) {
      if (!IS_FUNCTION(value) || desiredType != CONV_STRING) break;
      ObjString *functionName = AS_OBJ_FUNCTION(value)->name;
      return OBJECT_VAL(copyStringVal(&vm->objects, functionName->chars,
                                      functionName->length, &vm->strings));
    }

    // Convert string
    ObjString *string = AS_OBJ_STRING(value);

    if (desiredType == CONV_BOOL) return BOOL_VAL(isTruthy(value));
    if (desiredType == CONV_NUMBER) {
      if (string->length == 0) return NUMBER_VAL(0.0);
      char *endptr = NULL;
      double num = strtod(string->chars, &endptr);
      return NUMBER_VAL(num);
    }
    break;
  }
  }

  runtimeError("Cannot convert '%s' to '%s'.", VALUE_TYPE_NAMES[value.type],
               VALUE_TYPE_NAMES[convToValueType(desiredType)]);
  return NULL_VAL;
}

static ConversionResult
convertValues(VM *vm, Value *a, Value *b,
              const ConversionType priorities[CONVTYPE_COUNT]) {
  if ((a->type == b->type) &&
      (a->type != VAL_OBJECT || AS_OBJ(*a)->type == AS_OBJ(*b)->type))
    return CONVRES_OK;

  for (int i = 0; i < CONVTYPE_COUNT; i++) {
    switch (priorities[i]) {
    case CONV_NULL: {
      if (a->type == VAL_NULL || b->type == VAL_NULL) {
        runtimeError("Cannot operate on 'null' values.");
        return CONVRES_ERROR;
      }
      continue;
    }
    case CONV_BOOL: {
      if (a->type == VAL_BOOL || b->type == VAL_BOOL) {
        Value *toConvert = a->type == VAL_BOOL ? b : a;
        if (!canConvert(*toConvert, CONV_BOOL)) {
          runtimeError("Cannot convert '%s' to 'bool'.",
                       VALUE_TYPE_NAMES[toConvert->type]);
          return CONVRES_ERROR;
        }
        *toConvert = convertValue(vm, *toConvert, CONV_BOOL);
        return CONVRES_OK;
      }
      continue;
    }
    case CONV_NUMBER: {
      if (a->type == VAL_NUMBER || b->type == VAL_NUMBER) {
        Value *toConvert = a->type == VAL_NUMBER ? b : a;
        if (!canConvert(*toConvert, CONV_NUMBER)) {
          runtimeError("Cannot convert '%s' to a number.",
                       VALUE_TYPE_NAMES[toConvert->type]);
          return CONVRES_ERROR;
        }
        *toConvert = convertValue(vm, *toConvert, CONV_NUMBER);
        return CONVRES_OK;
      }
      continue;
    }
    case CONV_STRING: {
      if (IS_STRING(*a) || IS_STRING(*b)) {
        Value *toConvert = IS_STRING(*a) ? b : a;
        if (!canConvert(*toConvert, CONV_STRING)) {
          runtimeError("Cannot convert '%s' to a string.",
                       VALUE_TYPE_NAMES[toConvert->type]);
          return CONVRES_ERROR;
        }
        *toConvert = convertValue(vm, *toConvert, CONV_STRING);
        return CONVRES_OK;
      }
      continue;
    }
    }
  }
  // If we have neither two of the same type, a string and something else or a
  // number and a num/bool, we cannot convert them.
  return CONVRES_ERROR;
}

static Equality isEqual(VM *vm, Value a, Value b) {
  if (CONVRES_ERROR == convertValues(vm, &a, &b, EQUALITY_PRIORITIES)) {
    return EQ_ERROR;
  }
  switch (a.type) {
  case VAL_NULL: return a.type == b.type;
  case VAL_BOOL: return a.as.boolean == b.as.boolean ? EQ_EQUAL : EQ_NOT_EQUAL;
  case VAL_NUMBER: return a.as.number == b.as.number ? EQ_EQUAL : EQ_NOT_EQUAL;
  case VAL_OBJECT: {
    if (!IS_STRING(a) || !IS_STRING(b)) {
      runtimeError("Can only compare two string objects with eachother.");
      return EQ_ERROR;
    }
    ObjString *bStr = AS_OBJ_STRING(b);
    ObjString *aStr = AS_OBJ_STRING(a);
    return (aStr->hash == bStr->hash && aStr->length == bStr->length &&
            0 == memcmp(aStr->chars, bStr->chars, aStr->length))
               ? EQ_EQUAL
               : EQ_NOT_EQUAL;
  }
  }
}

static int concatenate(VM *vm, Value aVal, Value bVal) {
  ObjString *b = AS_OBJ_STRING(bVal);
  ObjString *a = AS_OBJ_STRING(aVal);

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(&vm->objects, chars, length, &vm->strings);
  push(vm, OBJECT_VAL(result));
}

static InterpretResult run(VM *vm) {
  CallFrame *frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() ((uint16_t)READ_BYTE() << 8 | READ_BYTE())
#define READ_CONSTANT_8()                                                      \
  (*(Value *)vecGet(&frame->function->chunk->constants, READ_BYTE()))
#define READ_CONSTANT_16()                                                     \
  (*(Value *)vecGet(&frame->function->chunk->constants, READ_SHORT()))
#define BINARY_OP(op, resultMacro, errorMsg)                                   \
  do {                                                                         \
    Value b = convertValue(vm, pop(vm), CONV_NUMBER);                          \
    Value a = convertValue(vm, pop(vm), CONV_NUMBER);                          \
    if (a.type == VAL_NULL || b.type == VAL_NULL) {                            \
      runtimeError(errorMsg);                                                  \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    push(vm, resultMacro(a.as.number op b.as.number));                         \
  } while (false)
#define UNARY_OP(op, errorMsg)                                                 \
  do {                                                                         \
    Value a = convertValue(vm, pop(vm), CONV_NUMBER);                          \
    if (a.type != VAL_NUMBER) {                                                \
      runtimeError(errorMsg);                                                  \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    (*(vm->stackTop - 1)).as.number = op(*(vm->stackTop - 1)).as.number;       \
  } while (false)
#define LOGIC_BINARY_OP(op, resultMacro, errorMsg)                             \
  do {                                                                         \
    Value b = convertValue(vm, pop(vm), CONV_NUMBER);                          \
    Value a = convertValue(vm, pop(vm), CONV_NUMBER);                          \
    if (a.type == VAL_NULL || b.type == VAL_NULL) {                            \
      runtimeError(errorMsg);                                                  \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    push(vm, resultMacro((double)(((long)a.as.number)op((long)b.as.number)))); \
  } while (false)

#define CHECK_TYPE(value, valueType, errorMsg)                                 \
  do {                                                                         \
    if (value.type != valueType) {                                             \
      runtimeError(errorMsg);                                                  \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
  } while (false)
#define POP_NUMBER() convertValue(vm, pop(vm), CONV_NUMBER)
#define CHECK_NUMBER(value, errorMsg)                                          \
  do {                                                                         \
    if (value.type == VAL_NULL) {                                              \
      runtimeError(errorMsg);                                                  \
      return INTERPRET_RUNTIME_ERROR;                                          \
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
      printInstruction(
          frame->function->chunk,
          (int)(frame->ip - (uint8_t *)frame->function->chunk->byteCode.arr));
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
      if (!tableGet(&vm->globals, AS_OBJ_STRING(identifierName), &val)) {
        runtimeError("Cannot access an undeclared variable.");
        return INTERPRET_RUNTIME_ERROR;
      }
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
      push(vm, frame->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(vm, 0);
      break;
    }
    case OP_PUSH_1: push(vm, NUMBER_VAL(1)); break;
    case OP_PRINT:  {
      Value val = pop(vm);
      printValue(&val);
      printf("\n");
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (!isTruthy(peek(vm, 0))) frame->ip += offset;
      break;
    }
    case OP_JUMP:         frame->ip += READ_SHORT(); break;
    case OP_LOOP:         frame->ip -= READ_SHORT(); break;

    case OP_POP:          pop(vm); break;
    case OP_CONVERT_TYPE: {
      push(vm, convertValue(vm, pop(vm), READ_BYTE()));
      break;
    }

    case OP_ADD: {
      Value b = pop(vm);
      Value a = pop(vm);
      if (CONVRES_ERROR == convertValues(vm, &a, &b, ADDITION_PRIORITIES))
        return INTERPRET_RUNTIME_ERROR;
      if (IS_STRING(a)) {
        concatenate(vm, a, b);
      } else if (a.type == VAL_NUMBER) {
        push(vm, NUMBER_VAL(a.as.number + b.as.number));
      } else if (a.type == VAL_BOOL) {
        push(vm, NUMBER_VAL((double)(a.as.boolean + b.as.boolean)));
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
      Value b = POP_NUMBER();
      CHECK_NUMBER(b, "The divisor of a division must be a number.");
      Value a = POP_NUMBER();
      CHECK_NUMBER(a, "The dividend of a division must be a number.");
      if (b.as.number == 0.0) {
        runtimeError("Cannot divide by zero.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(vm, NUMBER_VAL(a.as.number / b.as.number));
      break;
    }
    case OP_MODULO: {
      Value b = POP_NUMBER();
      CHECK_NUMBER(b, "The divisor of a modulo must be a number.");
      Value a = POP_NUMBER();
      CHECK_NUMBER(a, "The dividend of a modulo must be a number.");
      push(vm, NUMBER_VAL(fmod(a.as.number, b.as.number)));
      break;
    }
    case OP_NEGATE: UNARY_OP(-, "Can only negate numbers."); break;
    case OP_POWER:  {
      Value b = POP_NUMBER();
      CHECK_NUMBER(b, "The exponent of an exponentiation must be a number.");
      Value a = POP_NUMBER();
      CHECK_NUMBER(a, "The base of an exponentiation must be a number.");
      push(vm, NUMBER_VAL(pow(a.as.number, b.as.number)));
      break;
    }
    case OP_AND:
      LOGIC_BINARY_OP(&, NUMBER_VAL, "Can only and numbers or booleans.");
      break;
    case OP_NOT: {
      Value a = convertValue(vm, pop(vm), CONV_BOOL);
      if (a.type != VAL_BOOL) {
        runtimeError("Can only not a boolean.");
        return INTERPRET_RUNTIME_ERROR;
      }
      (*(vm->stackTop - 1)).as.boolean = !(*(vm->stackTop - 1)).as.boolean;
      break;
    }
    case OP_OR:
      LOGIC_BINARY_OP(|, NUMBER_VAL, "Can only or numbers or booleans.");
      break;
    case OP_XOR:
      LOGIC_BINARY_OP(^, NUMBER_VAL, "Can only xor numbers or booleans.");
      break;

    case OP_EQUALS: {
      Value b = pop(vm);
      Value a = pop(vm);
      Equality equal = isEqual(vm, a, b);
      if (equal == EQ_ERROR) return INTERPRET_RUNTIME_ERROR;
      push(vm, BOOL_VAL(equal));
      break;
    }
    case OP_CASE: {
      Value b = pop(vm);
      Value a = peek(vm, 0);
      Equality equal = isEqual(vm, a, b);
      if (equal == EQ_ERROR) return INTERPRET_RUNTIME_ERROR;
      push(vm, BOOL_VAL(equal));
      break;
    }
    case OP_NOT_EQUALS: {
      Value b = pop(vm);
      Value a = pop(vm);
      Equality equal = isEqual(vm, a, b);
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
      bool b = isTruthy(pop(vm));
      bool a = isTruthy(pop(vm));
      push(vm, BOOL_VAL(a & b));
      break;
    }
    case OP_OR_OR: {
      bool b = isTruthy(pop(vm));
      bool a = isTruthy(pop(vm));
      push(vm, BOOL_VAL(a | b));
    }

    // Arduino operations
    case OP_SETPIN: {
      Value value = pop(vm);
      Value pin = pop(vm);

      if (value.type != VAL_BOOL && value.type != VAL_NUMBER) {
        runtimeError("Can only set pins to HIGH/LOW (or 1/0).");
        return INTERPRET_RUNTIME_ERROR;
      }
      CHECK_TYPE(pin, VAL_NUMBER, "Can only write to valid pin numbers.");
      if ((int)pin.as.number > 13 || (int)pin.as.number < 0) {
        runtimeError("Invalid pin number '%d'.", (int)pin.as.number);
        return INTERPRET_RUNTIME_ERROR;
      }

      printf("Setting pin %d to %f\n", (int)pin.as.number, value.as.boolean);
      break;
    }
    case OP_PINMODE: {
      Value mode = pop(vm);
      Value pin = pop(vm);
      break;
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
#undef LOGIC_BINARY_OP
#undef UNARY_OP
#undef POP_NUMBER
#undef CHECK_NUMBER
}

InterpretResult interpret(VM *vm, ObjFunction *function) {
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(vm, OBJECT_VAL(function));
  CallFrame *frame = &vm->frames[vm->frameCount++];
  frame->function = function;
  frame->ip = (uint8_t *)function->chunk->byteCode.arr;
  frame->slots = vm->stack;

  return run(vm);
}

#endif