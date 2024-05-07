// RUN: echo '%s:26' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_aflgo_cc_test -O3 %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
// CHECK: entry:
// CHECK-NEXT: call void @__sanitizer_cov_trace_pc_guard
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 1000)
  int V = 0;
// CHECK: call void @__sanitizer_cov_trace_const_cmp8(i64 5, i64 %{{.+}})
// CHECK-NEXT: [[CMP:%.+]] = icmp ugt i64 %{{.+}}, 5
// CHECK-NEXT: br i1 [[CMP]], label %[[THEN:.+]], label %if.else
  if (Size > 5) {
// CHECK: [[THEN]]:
// CHECK-NEXT: call void @__sanitizer_cov_trace_pc_guard
    V = 1;
  } else {
// CHECK: if.else:
// CHECK-NEXT: call void @__sanitizer_cov_trace_pc_guard
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 0)
    V = Size + 2;
  }

  return V;
}