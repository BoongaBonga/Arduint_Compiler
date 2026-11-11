#ifndef ARDUINT_CHUNK_H
#define ARDUINT_CHUNK_H

#include <stdint.h>

#include "collection.h"
#include "value.h"

typedef enum {
  OP_ERROR = UINT8_MAX, // Skip
  OP_RETURN = 0,        // Return from current function
  OP_NO_OP,             // Do nothing
  OP_CONSTANT_8,        // Push a constant on the stack (u8)
  OP_CONSTANT_16,       // Push a constant on the stack (u16)
  OP_DEFINE_GLOBAL,     // Unused
  OP_GET_GLOBAL,        // Push global val on stack (u8)
  OP_SET_GLOBAL,        // Set a global (u8)
  OP_GET_LOCAL,         // Push a local value (offset u8) on the stack
  OP_SET_LOCAL,         // Set a value to a local with index u8 (nopop)
  OP_PRINT,             // Print stacktop wo popping to console

  OP_JUMP_IF_FALSE, // Jump if stackTop = false without popping (u16, fw)
  OP_JUMP,          // Jump forward u16 offset
  OP_LOOP,          // Jump backward u16 offset

  OP_ASSIGN, // Unused i think

  OP_POP, // Pop value off of stack

  OP_ADD,      // Add top two stack slots and push to stack
  OP_SUBTRACT, // Same but sub
  OP_MULTIPLY, // "
  OP_DIVIDE,   // "
  OP_MODULO,   // "
  OP_NEGATE,   // Negate top of stack
  OP_POWER,    // do lowest stackslot to the power of stacktop and push
  OP_AND,      // Bitwise and
  OP_NOT,      // Not stacktop
  OP_OR,       // Bitwise or
  OP_XOR,      // xor

  OP_EQUALS, // pushes true if top two slots are equal
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