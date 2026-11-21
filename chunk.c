#ifndef ARDUINT_CHUNK_C
#define ARDUINT_CHUNK_C

#include <stdint.h>

#include "chunk.h"
#include "collection.h"
#include "value.h"

void initChunk(Chunk *chunk) {
  chunk->byteCode = initVector(sizeof(uint8_t));
  chunk->constants = initVector(sizeof(Value));
  chunk->lines = initVector(sizeof(int));
}
void freeChunk(Chunk *chunk) {
  freeVector(&chunk->byteCode);
  freeVector(&chunk->constants);
  freeVector(&chunk->lines);
}

#endif