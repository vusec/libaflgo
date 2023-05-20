// RUN: echo '%s:28' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_aflgo_cc_test %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: mkdir -p %t.input
// RUN: echo 'yolo' > %t.input/yolo.txt
// RUN: rm -rf %t.output
// RUN: timeout --preserve-status 1s %t -i %t.input -o %t.output -l %t.log | %FileCheck %s

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  int V = 0;
  if (Data[0] > 100) {
    if (Data[0] > 200) {
      V = 1;
    } else {
      V = 3;
    }
  } else {
    V = 2;
  }

  int E = 1;
  if (Data[1] > 100) {
    if (Data[1] > 200) {
      E = 2;
    }
  }

  return V + E;
}

// CHECK: Let's fuzz :)
// CHECK: We imported 1 inputs from disk.
// CHECK: edges:
// CHECK-NOT: edges: 0/7