// RUN: printf '%s:30' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_hawkeye_cxx_test -fsanitize=address %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <stdint.h>
#include <stdlib.h>

// CHECK: @_ZTV1A = dso_local constant { { [5 x i8*] }, [56 x i8] }
// CHECK: @_ZTV1B = dso_local constant { { [5 x i8*] }, [56 x i8] }

class A {
public:
  virtual ~A();
  virtual int getVal() = 0;
};
A::~A() = default;

class B : public A {
public:
  ~B() {}
  int getVal() override;
};

// CHECK: define {{.*}} @_ZN1B6getValEv
int B::getVal() {
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 0.000000e+00)
// CHECK-NEXT: call void @__aflgo_trace_fun_distance(double 0.000000e+00)
    return 42;
  }

// CHECK: define {{.*}} @LLVMFuzzerTestOneInput
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.500000e+01)
// CHECK-NEXT: call void @__aflgo_trace_fun_distance(double 2.250000e+00)
// CHECK: call void @__sanitizer_cov_trace_pc_guard

  int V = 0;
  if (Size > 0 && Data[0] < 2) {
    B B;

// CHECK: if.then:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.000000e+01)
// CHECK: call void @__sanitizer_cov_trace_pc_guard
    V = B.getVal();
  } else {
// CHECK: if.else:
    V = 1337;
  }

  return V;
}