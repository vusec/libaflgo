#pragma once

#include <llvm/IR/PassManager.h>

namespace llvm {

class AFLGoFunctionDistanceAnalysis
    : public AnalysisInfoMixin<AFLGoFunctionDistanceAnalysis> {
public:
  static AnalysisKey Key;

  using Result = DenseMap<Function *, double>;

  Result run(Module &M, ModuleAnalysisManager &MAM);
};

} // namespace llvm