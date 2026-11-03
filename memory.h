#ifndef ARDUINT_MEMORY_H
#define ARDUINT_MEMORY_H

#include <stdlib.h>

#include "object.h"

#define ALLOCATE(type, count)                                                  \
  (type *)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define FREE_ARRAY(type, pointer, capacity)                                    \
  reallocate(pointer, sizeof(type) * capacity, 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

void freeObject(Object *object);
void freeObjects(Object *object);

#endif