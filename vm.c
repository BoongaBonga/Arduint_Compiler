#ifndef ARDUINT_VM_C
#define ARDUINT_VM_C

#include "vm.h"
#include "collection.h"
#include "object.h"
#include "value.h"

VM initVM() {
  VM vm;
  vm.stackTop = NULL;
  vm.objects = NULL;
  return vm;
}

void freeVM(VM *vm) { freeObjects(vm->objects); }

#endif