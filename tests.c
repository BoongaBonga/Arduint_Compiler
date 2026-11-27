#include <stdio.h>

#include "collection.h"
#include "debug.h"
#include "value.h"

#define TEST_N 10

int main() {
  printf("Starting tests\n");
  Value results[TEST_N];
  for (int i = 0; i < TEST_N; i++) {
    results[i] = NULL_VAL;
  }

  RingBuffer b = makeRingBuffer(sizeof(int), 5);

  // one insert
  {
    int a = 438047;
    bool c = ringBuffPush(&b, &a);

    ringBuffPop(&b, &a);
    results[0] = BOOL_VAL(a == 438047 && c);
  }

  // insert five, pop 5
  {
    int a[5] = {1, 2, 3, 4, 5};
    bool worked = ringBuffPushArr(&b, 5, a);

    int result = 0;
    int c;
    for (int i = 0; i < 5; i++) {
      ringBuffPop(&b, &c);
      printf("\n%d", c);
      result += c;
    }
    results[1] = BOOL_VAL(worked);
    results[2] = NUMBER_VAL((double)result);
  }

  for (int i = 0; i < TEST_N; i++) {
    printf("[");
    printValue(&results[i]);
    printf("] ");
  }
  freeRingBuffer(&b);
}