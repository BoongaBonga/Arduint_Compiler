#ifndef ARDUINT_MEMORY_C
#define ARDUINT_MEMORY_C

#include <stdio.h>

#include "memory.h"
#include "object.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

void freeObject(Object *object) {
  switch (object->type) {
  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    free(string->chars);
    FREE(ObjString, object);
    break;
  }
  }
}

#endif