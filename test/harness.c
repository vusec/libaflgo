// RUN: echo '%s:22' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_aflgo_cc_test %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
// CHECK: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 1000)
// CHECK: call void @__sanitizer_cov_trace_pc_guard
  int V = 0;
  if (Size > 5) {
// CHECK: call void @__sanitizer_cov_trace_pc_guard
    V = 1;
  } else {
// CHECK: if.else:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 0)
// CHECK: call void @__sanitizer_cov_trace_pc_guard
    V = 2;
  }

  return V;
}