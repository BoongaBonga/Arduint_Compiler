#ifndef ARDUINT_VALUE_H
#define ARDUINT_VALUE_H

#include <stdbool.h>

#include "object.h"

typedef struct Object Object;
typedef enum ObjType ObjType;
typedef struct ObjString ObjString;
typedef struct Value Value;

#define NUMBER_VAL(num) ((Value){VAL_NUMBER, {.number = (num)}})
#define BOOL_VAL(bool) ((Value){VAL_BOOL, {.boolean = (bool)}})
#define NULL_VAL ((Value){VAL_NULL, {0}})
#define OBJECT_VAL(obj) ((Value){VAL_OBJECT, {.object = (Object *)(obj)}})

typedef enum { VAL_BOOL, VAL_NUMBER, VAL_NULL, VAL_OBJECT } ValueType;

struct Value {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object *object;
  } as;
};

static inline bool isObjType(Value value, ObjType objType) {
  return value.type == VAL_OBJECT && value.as.object->type == objType;
}

#endif