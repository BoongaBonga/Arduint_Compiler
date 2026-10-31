#ifndef ARDUINT_ERRORHANDLING_H
#define ARDUINT_ERRORHANDLING_H

#include "token.h"

void defaultError(const char *message, ...);

void errorFromToken(Token *token, const char *message, ...);

#endif