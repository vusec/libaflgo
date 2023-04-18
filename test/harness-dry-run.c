// RUN: echo 'test/harness.c:15' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_cc_test %s -o %t
// RUN: echo 'yolo' > %t.input.txt
// RUN: %t %t.input.txt | %FileCheck %s

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  int V = 0;
  if (Size > 5) {
    V = 1;
  } else {
    V = 2;
  }

  return V;
}

// CHECK: You are not fuzzing, just executing 1 testcases
// CHECK-NEXT: Executing