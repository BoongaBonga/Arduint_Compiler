#ifndef ARDUINT_VALUE_H
#define ARDUINT_VALUE_H

#include <stdbool.h>

typedef enum { BOOL_VAL, NUMBER_VAL, NULL_VAL } ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;
} Value;

#endif