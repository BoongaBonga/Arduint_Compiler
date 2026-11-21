#ifndef ARDUINT_ERRORHANDLING_C
#define ARDUINT_ERRORHANDLING_C

#include <stdarg.h>
#include <stdio.h>

#include "token.h"
#include "vm.h"

void defaultError(const char *message, ...) {
  va_list args;
  va_start(args, message);

  fprintf(stderr, "(0-0:0-0) [Error]: ");
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  va_end(args);
}

void errorFromToken(Token *token, const char *message, ...) {
  va_list args;
  va_start(args, message);

  fprintf(stderr, "(%d-%d:%d-%d) [Error]: ", token->line, token->offset,
          token->line, token->offset + token->length - 1);
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  va_end(args);
}
void warningFromToken(Token *token, const char *message, ...) {
  va_list args;
  va_start(args, message);

  fprintf(stderr, "(%d-%d:%d-%d) [Warning]: ", token->line, token->offset,
          token->line, token->offset + token->length - 1);
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  va_end(args);
}

void customError(int startLine, int startOffset, int endLine, int endOffset,
                 const char *message, ...) {
  va_list args;
  va_start(args, message);

  fprintf(stderr, "(%d-%d:%d-%d) [Error]: ", startLine, startOffset, endLine,
          endOffset - 1);
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  va_end(args);
}

void runtimeError(VM *vm, const char *message, ...) {
  va_list args;
  va_start(args, message);

  CallFrame *frame = &vm->frames[vm->frameCount - 1];
  size_t instruction =
      frame->ip - (uint8_t *)frame->function->chunk->byteCode.arr - 1;
  int line = getLine(frame->function->chunk, instruction);

  fprintf(stderr, "(%d-?:%d-?) [Error]: ", line, line);
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");
  va_end(args);
}

#endif