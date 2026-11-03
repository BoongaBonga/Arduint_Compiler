#ifndef ARDUINT_OBJECT_C
#define ARDUINT_OBJECT_C

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "collection.h"
#include "errorHandling.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Object *allocateObject(size_t size, ObjType type) {
  Object *object = (Object *)reallocate(NULL, 0, size);
  object->type = type;
  object->next = NULL;
  return object;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash,
                                 Table *stringPool) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  tableSet(stringPool, string, NULL_VAL);
  return string;
}

uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString *copyStringVal(const char *chars, int length, Table *stringPool) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(stringPool, chars, length, hash);

  if (interned != NULL) return interned;

  char *heapChars = ALLOCATE(char, length + 1);
  if (heapChars == NULL) {
    defaultError("Did not have enough space to allocate string.");
    return NULL;
  }

  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash, stringPool);
}

ObjString *takeString(char *chars, int length, Table *stringPool) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(stringPool, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash, stringPool);
}

#endif