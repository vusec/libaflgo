// RUN: printf '%s:33' > %t.targets.txt
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
// CHECK-NEXT: {{.*}} = alloca
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 0)
// CHECK-DAG: call void @__aflgo_trace_fun_distance(double 0.000000e+00)
// CHECK: ret i32
    return 42;
  }

// CHECK: define {{.*}} @LLVMFuzzerTestOneInput
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
// CHECK-NEXT: entry:
// CHECK-NEXT-4: {{.*}} = alloca
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 15000)
// CHECK-DAG: call void @__aflgo_trace_fun_distance(double 2.250000e+00)

  int V = 0;
// CHECK: call void @__sanitizer_cov_trace_const_cmp4(i32 0, i32 {{.+}})
// CHECK-NEXT: %{{.+}} = icmp ne i32 %{{.+}}, 0
// CHECK-NEXT: br i1 %{{.+}}
  if (Size > 0 && Data[0] < 2) {
    B B;

// CHECK: if.then:
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 10000)
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
    V = B.getVal();
  } else {
// CHECK: if.else:
    V = 1337;
  }

  return V;
}