#ifndef ARDUINT_CHUNK_H
#define ARDUINT_CHUNK_H

#include <stdint.h>

#include "common.h"
#include "value.h"

typedef enum {
  OP_RETURN,
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