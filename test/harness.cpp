// RUN: echo '%s:46' > %t.targets.txt
// RUN: AFLGO_TARGETS=%t.targets.txt %libaflgo_aflgo_cxx_test %s -o %t -Wl,-save-temps
// RUN: %llvm-dis %t.0.5.precodegen.bc
// RUN: cat %t.0.5.precodegen.ll | %FileCheck %s

#include <cstdint>
#include <cstdlib>
#include <memory>

class Base {
public:
  virtual int getRetVal(int) = 0;
  virtual ~Base() {}
};

class Sub1 : public Base {
  int getRetVal(int X) override {
    return X + 1;
  }
};

class Sub2 : public Base {
public: 
  int getRetVal(int X) override {
// CHECK: define {{.*}} i32 @_ZN4Sub29getRetValEi
// CHECK-NEXT: entry:
// CHECK-NEXT: alloca
// CHECK-NEXT: alloca
// CHECK-NEXT: alloca
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 10000)
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
// CHECK: call {{.*}} @_ZN4Sub213computeRetValEi
    int Ret = computeRetVal(X);
// CHECK: ret i32
    return Ret;
  }

private:
// CHECK: define {{.*}} i32 @_ZN4Sub213computeRetValEi
  int computeRetVal(int X) {
// CHECK-NEXT: entry:
// CHECK-NEXT: alloca
// CHECK-DAG: call void @__sanitizer_cov_trace_pc_guard
// CHECK-DAG: call void @__aflgo_trace_bb_distance(i64 0)
// CHECK: ret i32
    return X + 2;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  std::unique_ptr<Base> B = nullptr;
  if (Size > 0 && Data[0] > 128) {
    B = std::make_unique<Sub1>();
  } else {
    B = std::make_unique<Sub2>();
  }

  return B->getRetVal(Size);
}