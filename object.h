#ifndef ARDUINT_OBJECT_H
#define ARDUINT_OBJECT_H

#include <stdint.h>

typedef struct Table Table;

typedef struct Object Object;
typedef enum ObjType ObjType;
typedef struct ObjString ObjString;

#define OBJ_TYPE(value) ((value).as.object->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_OBJ_STRING(value) ((ObjString *)((value).as.object))

enum ObjType {
  OBJ_STRING,
  OBJ_FUNCTION,
};

struct Object {
  ObjType type;
  struct Object *next;
};

struct ObjString {
  Object object;
  int length;
  char *chars;
  uint32_t hash;
};

/// @brief copies a string as a value, will be garbage-collected
ObjString *copyStringVal(Object **objs, const char *chars, int length,
                         Table *stringPool);

ObjString *takeString(Object **objs, char *chars, int length,
                      Table *stringPool);

#endif