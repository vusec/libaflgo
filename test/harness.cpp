// RUN: echo '%s:39' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_aflgo_cxx_test %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <cstdint>
#include <cstdlib>
#include <memory>

class Base {
public:
  virtual int getRetVal() = 0;
  virtual ~Base() {}
};

class Sub1 : public Base {
  int getRetVal() override {
    return 1;
  }
};

class Sub2 : public Base {
public: 
  int getRetVal() override {
// CHECK: define {{.*}} i32 @_ZN4Sub29getRetValEv
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 1.000000e+01)
    int Ret = computeRetVal();
// CHECK: call void @__sanitizer_cov_trace_pc_guard
    return Ret;
  }

private:
// CHECK: define {{.*}} void @_ZN4Sub213computeRetValEv
  int computeRetVal() {
// CHECK-NEXT: entry:
// CHECK-NEXT: call void @__aflgo_trace_bb_distance(double 0.000000e+00)
// CHECK: call void @__sanitizer_cov_trace_pc_guard
    return 2;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  std::unique_ptr<Base> B = nullptr;
  if (Size > 0 && Data[0] > 128) {
    B = std::make_unique<Sub1>();
  } else {
    B = std::make_unique<Sub2>();
  }

  return B->getRetVal();
}