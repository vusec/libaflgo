#pragma once

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/PassManager.h>

namespace llvm {

class AFLGoTargetDetectionAnalysis
    : public AnalysisInfoMixin<AFLGoTargetDetectionAnalysis> {
public:
  using Result = SmallVector<std::pair<BasicBlock *, CallBase *>, 16>;

  constexpr static const char *const TargetFunctionName =
      "__aflgo_trace_bb_target";
  static AnalysisKey Key;

  Result run(Function &F, FunctionAnalysisManager &FAM);
};

} // namespace llvm