// RUN: printf '%s:14\n%s:22' > %t.targets.txt
// RUN: AFLGO_HAWKEYE=true AFLGO_TARGETS=%t.targets.txt %libaflgo_hawkeye_cc_test %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <stdint.h>
#include <stdlib.h>

// CHECK: define {{.*}} @target1
int target1(void) {
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 0.000000e+00)
// CHECK-NEXT: call void @__aflgo_trace_fun_distance(double 0.000000e+00)
	return 1;
}

// CHECK: define {{.*}} @target2
int target2(void) {
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 0.000000e+00)
// CHECK-NEXT: call void @__aflgo_trace_fun_distance(double 0.000000e+00)
	return 2;
}

// CHECK: define {{.*}} @intermediate
int intermediate2(void) {
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.000000e+01)
// CHECK-NEXT: call void @__aflgo_trace_fun_distance(double 2.250000e+00)
	return target2();
}

// CHECK: define {{.*}} @LLVMFuzzerTestOneInput
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.200000e+01)
// CHECK-NEXT: call void @__aflgo_trace_fun_distance(double 3.000000e+00)
// CHECK: call void @__sanitizer_cov_trace_pc_guard
  typedef int (*val_cb)(void);

  val_cb Callbacks[2] = {
      target1,
      intermediate2,
  };

  int V = 0;
  if (Size > 0 && Data[0] < 2) {
// CHECK: land.lhs.true:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.100000e+01)

// CHECK: if.then:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.000000e+01)
// CHECK: call void @__sanitizer_cov_trace_pc_guard
    V = Callbacks[Data[0]]();
  } else {
// CHECK: if.else:
    V = 42;
  }

  return V;
}
