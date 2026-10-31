#ifndef ARDUINT_COMMON_H
#define ARDUINT_COMMON_H

#include <stdlib.h>

#include "memory.h"

typedef struct {
  size_t elementSize;
  void *arr;
  unsigned int capacity;
  unsigned int count;
} Vector;

Vector initVector(size_t elementSize);

void vecPush(Vector *vec, const void *value);

void vecPop(Vector *vec, void *target);

void *vecGet(const Vector *vec, unsigned int index);

void freeVector(Vector *vec);

#endif