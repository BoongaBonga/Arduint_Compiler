#ifndef ARDUINT_VM_H
#define ARDUINT_VM_H

#include "collection.h"
#include "object.h"
#include "value.h"

typedef struct {
  Value *stackTop;
  Object *objects;
} VM;

VM initVM();
void freeVM(VM *vm);

#endif