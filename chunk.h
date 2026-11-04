#ifndef ARDUINT_CHUNK_H
#define ARDUINT_CHUNK_H

#include <stdint.h>

#include "collection.h"
#include "value.h"

typedef enum {
  OP_ERROR = UINT8_MAX,
  OP_RETURN = 0,
  OP_NO_OP,
  OP_CONSTANT_8,
  OP_CONSTANT_16,
  OP_DEFINE_GLOBAL_8,
  OP_DEFINE_GLOBAL_16,
  OP_GET_GLOBAL_8,
  OP_GET_GLOBAL_16,
  OP_SET_GLOBAL_8,
  OP_SET_GLOBAL_16,
  OP_PRINT,
  OP_ASSIGN,

  OP_POP,

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

static inline uint16_t writeConstant(Chunk *chunk, Value constant) {
  vecPush(&chunk->constants, &constant);
  return chunk->constants.count - 1;
}

static inline Value *getConstant(Chunk *chunk, int index) {
  return (Value *)vecGet(&chunk->constants, index);
}

#endif