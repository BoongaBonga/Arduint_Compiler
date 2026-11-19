#ifndef ARDUINT_OBJECT_H
#define ARDUINT_OBJECT_H

#include <stdint.h>

typedef struct Table Table;
typedef struct Chunk Chunk;

typedef struct Object Object;
typedef enum ObjType ObjType;
typedef struct ObjString ObjString;

#define OBJ_TYPE(value) ((value).as.object->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)

#define AS_OBJ(value) ((value).as.object)

#define AS_OBJ_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_OBJ_STRING(value) ((ObjString *)AS_OBJ(value))

enum ObjType {
  OBJ_STRING,
  OBJ_FUNCTION,
};

struct Object {
  ObjType type;
  struct Object *next;
};

typedef struct {
  Object object;
  int arity;
  Chunk *chunk;
  ObjString *name;
} ObjFunction;

struct ObjString {
  Object object;
  int length;
  char *chars;
  uint32_t hash;
};

ObjFunction *newFunction(Object **objs);

/// @brief copies a string as a value, will be garbage-collected
ObjString *copyStringVal(Object **objs, const char *chars, int length,
                         Table *stringPool);

ObjString *takeString(Object **objs, char *chars, int length,
                      Table *stringPool);

#endif