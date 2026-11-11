#ifndef ARDUINT_COLLECTION_C
#define ARDUINT_COLLECTION_C

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collection.h"
#include "errorHandling.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

Vector initVector(size_t elementSize) {
  Vector vec;
  vec.elementSize = elementSize;
  vec.arr = NULL;
  vec.capacity = 0;
  vec.count = 0;

  return vec;
}

void vecPush(Vector *vec, const void *value) {
  // Check if we need to resize FIRST, before writing
  if (vec->count >= vec->capacity) {
    size_t oldSize = vec->capacity * vec->elementSize;
    vec->capacity = vec->capacity < 8 ? 8 : vec->capacity * 2;
    size_t newSize = vec->capacity * vec->elementSize;

    vec->arr = reallocate(vec->arr, oldSize, newSize);
    if (vec->arr == NULL) {
      defaultError("Not enough memory to reallocate vector.");
      exit(1);
    }
  }

  // Now safe to copy into the current count index
  char *target = (char *)vec->arr + (vec->count * vec->elementSize);
  memcpy(target, value, vec->elementSize);
  vec->count++;
}

void vecPop(Vector *vec, void *target) {
  vec->count--;

  char *source = (char *)vec->arr + (vec->count * vec->elementSize);

  if (target != NULL) memcpy(target, source, vec->elementSize);

  if (vec->capacity > 8 && vec->count < vec->capacity / 4) {

    vec->arr = reallocate(vec->arr, vec->capacity * vec->elementSize,
                          (vec->capacity * vec->elementSize) / 2);
    vec->capacity /= 2;
  }
}

void *vecGet(const Vector *vec, unsigned int index) {
  if (index >= vec->count) {
    defaultError("Index %u outside of range.", index);
    return NULL;
  }
  return (char *)vec->arr + (index * vec->elementSize);
}

void freeVector(Vector *vec) {
  reallocate(vec->arr, vec->capacity * vec->elementSize, 0);
  vec->arr = NULL;
  vec->capacity = 0;
  vec->count = 0;
}

//____________________________
//        Tables
//____________________________

Table initTable() {
  Table table;
  table.count = 0;
  table.capacity = 0;
  table.entries = NULL;
  return table;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
  uint32_t index = key->hash % capacity;
  Entry *tombstone = NULL;

  for (;;) {
    Entry *entry = &entries[index];
    if (entry->key == NULL) {
      if (entry->value.type == VAL_NULL) {
        // Empty entry
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (entry->key == key) {
      // We found the key
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void adjustCapacity(Table *table, int capacity) {
  Entry *entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NULL_VAL;
  }

  Entry *oldEntries = table->entries;
  int oldCapacity = table->capacity;
  table->count = 0;
  for (int i = 0; i < oldCapacity; i++) {
    Entry *entry = &oldEntries[i];
    if (entry->key == NULL) continue;

    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, oldEntries, oldCapacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableGet(Table *table, ObjString *key, Value *value) {
  if (table->count == 0) return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  if (value != NULL) *value = entry->value;
  return true;
}

bool tableSet(Table *table, ObjString *key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
    adjustCapacity(table, capacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;
  if (isNewKey && entry->value.type == VAL_NULL) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(Table *table, ObjString *key) {
  if (table->count == 0) return false;

  // Find the entry
  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

void tableAddAll(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry *entry = &table->entries[index];
    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry.
      if (entry->value.type == VAL_NULL) return NULL;
    } else if (entry->key->length == length && entry->key->hash == hash &&
               memcmp(entry->key->chars, chars, length) == 0) {
      // We found it
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}

//_______________________GENERIC TABLES____________________

GenericTable initGenericTable(size_t elementSize) {
  GenericTable table;
  table.count = 0;
  table.capacity = 0;
  table.elementSize = elementSize;
  table.values = NULL;
  table.entries = NULL;
  return table;
}

void freeGenericTable(GenericTable *table) {
  reallocate(table->entries, sizeof(GenericEntry) * table->capacity, 0);
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
  free(table->values);
  table->values = NULL;
}

static GenericEntry *findGenericEntry(GenericEntry *entries, int capacity,
                                      ObjString *key) {
  uint32_t index = key->hash % capacity;
  GenericEntry *tombstone = NULL;

  for (;;) {
    GenericEntry *entry = &entries[index];
    if (entry->key == NULL) {
      if (entry->value == NULL) {
        // Empty entry
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (entry->key == key) {
      // We found the key
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void genericAdjustCapacity(GenericTable *table, int capacity) {
  // Adjust values array
  void *oldValues = table->values;
  void *values = malloc(capacity * table->elementSize);

  GenericEntry *entries = ALLOCATE(GenericEntry, capacity);

  if (entries == NULL || values == NULL) {
    defaultError("Failed to allocate table.");
    exit(74);
  }

  memset(values, 0, table->elementSize * capacity);

  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NULL;
  }

  GenericEntry *oldEntries = table->entries;
  int oldCapacity = table->capacity;
  table->count = 0;
  for (int i = 0; i < oldCapacity; i++) {
    GenericEntry *entry = &oldEntries[i];
    if (entry->key == NULL) continue;

    GenericEntry *dest = findGenericEntry(entries, capacity, entry->key);
    dest->key = entry->key;

    // copy over the value from the old value array
    int newIndex = (int)(dest - entries);
    void *newValue = (char *)values + (size_t)newIndex * table->elementSize;
    void *oldValue = (char *)oldValues + (size_t)i * table->elementSize;

    memcpy(newValue, oldValue, table->elementSize);
    // And then set the new entry's value to a reference to that new value
    dest->value = newValue;
    table->count++;
  }

  FREE_ARRAY(GenericEntry, oldEntries, oldCapacity);
  table->entries = entries;
  table->capacity = capacity;

  free(table->values);
  table->values = values;
}

bool genericTableGet(GenericTable *table, ObjString *key, void *value) {
  if (table->count == 0) return false;

  GenericEntry *entry = findGenericEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  if (value != NULL) memcpy(value, entry->value, table->elementSize);
  return true;
}

bool genericTableSet(GenericTable *table, ObjString *key, void *value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
    genericAdjustCapacity(table, capacity);
  }

  GenericEntry *entry = findGenericEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;
  if (isNewKey && entry->value == NULL) table->count++;

  entry->key = key;

  if (entry->value == NULL) {
    entry->value =
        (char *)table->values + (entry - table->entries) * table->elementSize;
  }

  memcpy(entry->value, value, table->elementSize);
  return isNewKey;
}

bool genericTableDelete(GenericTable *table, ObjString *key) {
  if (table->count == 0) return false;

  // Find the entry
  GenericEntry *entry = findGenericEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  entry->key = NULL;
  return true;
}

void genericTableAddAll(GenericTable *from, GenericTable *to) {
  for (int i = 0; i < from->capacity; i++) {
    GenericEntry *entry = &from->entries[i];
    if (entry->key != NULL) {
      genericTableSet(to, entry->key, entry->value);
    }
  }
}

ObjString *genericTableFindString(GenericTable *table, const char *chars,
                                  int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    GenericEntry *entry = &table->entries[index];
    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry.
      if (entry->value == NULL) return NULL;
    } else if (entry->key->length == length && entry->key->hash == hash &&
               memcmp(entry->key->chars, chars, length) == 0) {
      // We found it
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}

#endif