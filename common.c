#ifndef ARDUINT_COMMON_C
#define ARDUINT_COMMON_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "errorHandling.h"
#include "memory.h"

Vector initVector(size_t elementSize) {
  Vector vec;
  vec.elementSize = elementSize;
  vec.arr = malloc(8 * elementSize);
  vec.capacity = 8;
  vec.count = 0;

  if (vec.arr == NULL) {
    defaultError("Not enough memory to allocate array.");
    exit(1);
  }
  return vec;
}

void vecPush(Vector *vec, const void *value) {
  // Check if we need to resize FIRST, before writing
  if (vec->count >= vec->capacity) {
    size_t oldSize = vec->capacity * vec->elementSize;
    vec->capacity = vec->capacity < 8 ? 8 : vec->capacity * 2;
    size_t newSize = vec->capacity * vec->elementSize;

    vec->arr = reallocate(vec->arr, oldSize, newSize);
    if (vec->arr == NULL) {
      defaultError("Not enough memory to reallocate vector.");
      exit(1);
    }
  }

  // Now safe to copy into the current count index
  char *target = (char *)vec->arr + (vec->count * vec->elementSize);
  memcpy(target, value, vec->elementSize);
  vec->count++;
}

void vecPop(Vector *vec, void *target) {
  vec->count--;

  char *source = (char *)vec->arr + (vec->count * vec->elementSize);

  if (target != NULL) memcpy(target, source, vec->elementSize);

  if (vec->capacity > 8 && vec->count < vec->capacity / 4) {

    vec->arr = reallocate(vec->arr, vec->capacity * vec->elementSize,
                          (vec->capacity * vec->elementSize) / 2);
    vec->capacity /= 2;
  }
}

void *vecGet(const Vector *vec, unsigned int index) {
  if (index >= vec->count) {
    defaultError("Index %u outside of range.", index);
    return NULL;
  }
  return (char *)vec->arr + (index * vec->elementSize);
}

void freeVector(Vector *vec) {
  reallocate(vec->arr, vec->capacity * vec->elementSize, 0);
  vec->arr = NULL;
  vec->capacity = 0;
  vec->count = 0;
}

#endif