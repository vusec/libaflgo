#pragma once

#include <Analysis/FunctionDistance.hpp>

#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/PassManager.h>

namespace llvm {

class AFLGoBasicBlockDistanceAnalysis
    : public AnalysisInfoMixin<AFLGoBasicBlockDistanceAnalysis> {
public:
  static AnalysisKey Key;

  class Result {
  public:
    using FunctionToDistanceTy = AFLGoFunctionDistanceAnalysis::Result;
    using BBToDistanceTy = SmallDenseMap<BasicBlock *, double, 16>;
    using FunctionToOriginBBsMapTy = DenseMap<Function *, BBToDistanceTy>;

  private:
    const FunctionToDistanceTy FunctionToDistance;
    FunctionToOriginBBsMapTy FunctionToOriginBBs;

  public:
    explicit Result(FunctionToOriginBBsMapTy &FunctionToOriginBBs,
                    const FunctionToDistanceTy &FunctionToDistance)
        : FunctionToOriginBBs(FunctionToOriginBBs),
          FunctionToDistance(FunctionToDistance) {}

    BBToDistanceTy computeBBDistances(Function &F);
  };

  Result run(Module &F, ModuleAnalysisManager &FAM);
};

} // namespace llvm