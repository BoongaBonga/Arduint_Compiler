#ifndef ARDUINT_OBJECT_C
#define ARDUINT_OBJECT_C

#include <stdio.h>
#include <string.h>

#include "errorHandling.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Object *allocateObject(size_t size, ObjType type) {
  Object *object = (Object *)reallocate(NULL, 0, size);
  object->type = type;
  return object;
}

static ObjString *allocateString(char *chars, int length) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

ObjString *copyStringVal(const char *chars, int length) {
  char *heapChars = ALLOCATE(char, length + 1);
  if (heapChars == NULL) {
    defaultError("Did not have enough space to allocate string.");
    return NULL;
  }

  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length);
}

#endif