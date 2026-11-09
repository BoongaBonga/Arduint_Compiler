#ifndef ARDUINT_COLLECTION_H
#define ARDUINT_COLLECTION_H

#include <stdbool.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "value.h"

typedef struct Table Table;
typedef struct GenericTable GenericTable;

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

typedef struct {
  ObjString *key;
  void *value; // Value is 1 if it's a tombstone
} GenericEntry;

struct GenericTable {
  int count;
  int capacity;
  size_t elementSize;
  void *values;
  GenericEntry *entries;
};

GenericTable initGenericTable(size_t elementSize);
void freeGenericTable(GenericTable *table);
bool genericTableGet(GenericTable *table, ObjString *key, void *value);
bool genericTableSet(GenericTable *table, ObjString *key, void *value);
bool genericTableDelete(GenericTable *table, ObjString *key);
void genericTableAddAll(GenericTable *from, GenericTable *to);
ObjString *genericTableFindString(GenericTable *table, const char *chars,
                                  int length, uint32_t hash);

#endif