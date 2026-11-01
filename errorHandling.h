#ifndef ARDUINT_ERRORHANDLING_H
#define ARDUINT_ERRORHANDLING_H

#include "token.h"

void defaultError(const char *message, ...);

void errorFromToken(Token *token, const char *message, ...);

void customError(int startLine, int startOffset, int endLine, int endOffset,
                 const char *message, ...);

#endif