#ifndef ARDUINT_ERRORHANDLING_C
#define ARDUINT_ERRORHANDLING_C

#include <stdarg.h>
#include <stdio.h>

#include "token.h"

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

  fprintf(stderr, "(%d-%td:%d-%td) [Error]: ", token->line, token->offset,
          token->line, token->offset + token->length);
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  va_end(args);
}

#endif