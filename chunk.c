#ifndef ARDUINT_CHUNK_C
#define ARDUINT_CHUNK_C

#include <stdint.h>

#include "chunk.h"
#include "common.h"
#include "value.h"

Chunk initChunk() {
  Chunk chunk;
  chunk.byteCode = initVector(sizeof(uint8_t));
  chunk.constants = initVector(sizeof(Value));
  return chunk;
}
void freeChunk(Chunk *chunk) {
  freeVector(&chunk->byteCode);
  freeVector(&chunk->constants);
}

#endif