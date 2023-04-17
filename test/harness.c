// RUN: echo 'test/harness.c:22' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_cc_test %s -o %t -Wl,-save-temps
// RUN: llvm-dis-16 %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
// CHECK: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.000000e+00)
// CHECK call void @__sanitizer_cov_trace_pc_guard
  int V = 0;
  if (Size > 5) {
// CHECK call void @__sanitizer_cov_trace_pc_guard
    V = 1;
  } else {
// CHECK: if.else:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 0.000000e+00)
// CHECK call void @__sanitizer_cov_trace_pc_guard
    V = 2;
  }

  return V;
}