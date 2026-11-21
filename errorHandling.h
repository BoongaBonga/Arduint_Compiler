#ifndef ARDUINT_ERRORHANDLING_H
#define ARDUINT_ERRORHANDLING_H

#include "token.h"

typedef struct VM VM;

void defaultError(const char *message, ...);

void errorFromToken(Token *token, const char *message, ...);
void warningFromToken(Token *token, const char *message, ...);

void customError(int startLine, int startOffset, int endLine, int endOffset,
                 const char *message, ...);

void runtimeError(VM *vm, const char *message, ...);

#endif