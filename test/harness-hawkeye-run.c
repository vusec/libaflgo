// RUN: printf '%s:13\n%s:17' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_hawkeye_cc_test %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: mkdir -p %t.input
// RUN: echo 'yolo' > %t.input/yolo.txt
// RUN: rm -rf %t.output
// RUN: timeout --preserve-status 1s %t -D -i %t.input -o %t.output -l %t.log | %FileCheck %s

#include <stdint.h>
#include <stdlib.h>

int target1(void) {
	return 1;
}

int target2(void) {
	return 2;
}

int intermediate2(void) {
	return target2();
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  typedef int (*val_cb)(void);

  val_cb Callbacks[2] = {
      target1,
      intermediate2,
  };

  int V = 0;
  if (Size > 0 && Data[0] < 2) {
    V = Callbacks[Data[0]]();
  } else {
    V = 42;
  }

  return V;
}

// CHECK: Let's fuzz :)
// CHECK: We imported 1 inputs from disk.
// CHECK: edges:
// CHECK: similarity_min:
// CHECK-NOT: edges: 0/7