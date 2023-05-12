#include <AFLGoLinker/DistanceInstrumentation.hpp>
#include <Analysis/BasicBlockDistance.hpp>

#include <llvm/IR/IRBuilder.h>

using namespace llvm;

const char *AFLGoTraceBBDistanceName = "__aflgo_trace_bb_distance";

PreservedAnalyses
AFLGoDistanceInstrumentationPass::run(Module &M, ModuleAnalysisManager &AM) {
  auto &C = M.getContext();
  auto *VoidTy = Type::getVoidTy(C);
  auto *DoubleTy = Type::getDoubleTy(C);
  auto AFLGoTraceBBDistance =
      M.getOrInsertFunction(AFLGoTraceBBDistanceName, VoidTy, DoubleTy);

  auto &BBDistanceResult = AM.getResult<AFLGoBasicBlockDistanceAnalysis>(M);

  for (auto &F : M) {
    if (F.isDeclaration()) {
      continue;
    }

    auto BBDistances = BBDistanceResult.computeBBDistances(F);
    for (auto &BB : F) {
      if (BBDistances.find(&BB) == BBDistances.end()) {
        continue;
      }

      auto *DistanceValue = ConstantFP::get(DoubleTy, BBDistances[&BB]);
      IRBuilder<> IRB(&*BB.getFirstInsertionPt());
      IRB.CreateCall(AFLGoTraceBBDistance, {DistanceValue});
    }
  }

  return PreservedAnalyses::none();
}