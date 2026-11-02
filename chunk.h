#ifndef ARDUINT_CHUNK_H
#define ARDUINT_CHUNK_H

#include <stdint.h>

#include "common.h"
#include "value.h"

typedef enum {
  OP_ERROR = UINT8_MAX,
  OP_RETURN = 0,
  OP_NO_OP,
  OP_CONSTANT_8,
  OP_CONSTANT_16,
  OP_ASSIGN,

  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MODULO,
  OP_NEGATE,
  OP_POWER,
  OP_AND,
  OP_NOT,
  OP_OR,
  OP_XOR,

  OP_EQUALS,
  OP_NOT_EQUALS,
  OP_GREATER,
  OP_GREATER_OR_EQUAL,
  OP_LESS,
  OP_LESS_OR_EQUAL,
  OP_AND_AND,
  OP_OR_OR,

  OP_MINUS_EQUALS,
  OP_PLUS_EQUALS,
  OP_DIVIDE_EQUALS,
  OP_MODULO_EQUALS,
  OP_MULTIPLY_EQUALS,
  OP_POWER_EQUALS,
  OP_AND_EQUALS,
  OP_OR_EQUALS,
  OP_POST_DECREMENT,
  OP_POST_INCREMENT,
  OP_PRE_DECREMENT,
  OP_PRE_INCREMENT,
} OpCode;

/// @brief Bytecode chunk of uint8_t's
typedef struct {
  Vector constants; // Values
  Vector byteCode;  // uint8_ts
} Chunk;

Chunk initChunk();
void freeChunk(Chunk *chunk);

static inline void writeOperation(Chunk *chunk, uint8_t opCode) {
  vecPush(&chunk->byteCode, &opCode);
}

static inline uint8_t getOperation(Chunk *chunk, int index) {
  return *(uint8_t *)vecGet(&chunk->byteCode, index);
}

static inline void writeConstant(Chunk *chunk, Value constant) {
  vecPush(&chunk->constants, &constant);
}

static inline Value *getConstant(Chunk *chunk, int index) {
  return (Value *)vecGet(&chunk->constants, index);
}

#endif