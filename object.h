#ifndef ARDUINT_OBJECT_H
#define ARDUINT_OBJECT_H

#include <stdbool.h>

#include "common.h"

typedef struct Object Object;
typedef struct ObjString ObjString;
typedef struct Value Value;

#define OBJ_TYPE(value) ((value).as.object->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_OBJ_STRING(value) ((ObjString *)((value).as.object))

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
} ObjType;

struct Object {
  ObjType type;
  // struct Object *next;
};

struct ObjString {
  Object object;
  int length;
  char *chars;
};

/// @brief copies a string as a value, will be garbage-collected
ObjString *copyStringVal(const char *chars, int length);

static inline bool isObjType(Value *value, ObjType objType);

#endif