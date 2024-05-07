// RUN: printf '%s:20\n%s:25' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_aflgo_cc_test %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
// CHECK: entry:
// CHECK-COUNT-3: {{.*}} = alloca
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 1000)
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
  int V = 0;
  if (Size > 5) {
// CHECK: if.then:
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 0)
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
    V = 1;
  } else {
// CHECK: if.else:
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 0)
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
    V = 2;
  }

  return V;
}