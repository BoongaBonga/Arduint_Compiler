#ifndef ARDUINT_COLLECTION_H
#define ARDUINT_COLLECTION_H

#include <stdbool.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "value.h"

typedef struct Table Table;

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

//____________________________
//        Tables
//____________________________

typedef struct {
  ObjString *key;
  Value value;
} Entry;

struct Table {
  int count;
  int capacity;
  Entry *entries;
};

Table initTable();
void freeTable(Table *table);
bool tableGet(Table *table, ObjString *key, Value *value);
bool tableSet(Table *table, ObjString *key, Value value);
bool tableDelete(Table *table, ObjString *key);
void tableAddAll(Table *from, Table *to);
ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash);

#endif