#include <Analysis/TargetDetection.hpp>

#include <llvm/IR/InstrTypes.h>
#include <utility>

using namespace llvm;

AnalysisKey AFLGoTargetDetectionAnalysis::Key;

AFLGoTargetDetectionAnalysis::Result
AFLGoTargetDetectionAnalysis::run(Function &F, FunctionAnalysisManager &FAM) {
  SmallVector<std::pair<BasicBlock *, CallBase *>> Targets;
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *CB = dyn_cast<CallBase>(&I)) {
        auto *Callee = CB->getCalledFunction();
        if (!Callee) {
          continue;
        }

        if (Callee->getName().equals(TargetFunctionName)) {
          Targets.push_back(std::make_pair(&BB, CB));
          break;
        }
      }
    }
  }

  return Targets;
}
