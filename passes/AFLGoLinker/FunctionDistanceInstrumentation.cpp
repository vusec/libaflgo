#include <AFLGoLinker/FunctionDistanceInstrumentation.hpp>
#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

const char *AFLGoTraceFunDistanceName = "__aflgo_trace_fun_distance";

PreservedAnalyses FunctionDistancePass::run(Module &M,
                                            ModuleAnalysisManager &MAM) {
  auto &C = M.getContext();
  auto *VoidTy = Type::getVoidTy(C);
  auto *DoubleTy = Type::getDoubleTy(C);
  auto AFLGoTraceFunDistance =
      M.getOrInsertFunction(AFLGoTraceFunDistanceName, VoidTy, DoubleTy);

  auto FunctionDistances = MAM.getResult<AFLGoFunctionDistanceAnalysis>(M);
  for (auto &Entry : FunctionDistances) {
    auto *Function = Entry.first;
    auto FunDistance = Entry.second;

    auto *FunDistanceValue = ConstantFP::get(DoubleTy, FunDistance);
    IRBuilder<> IRB(&*Function->getEntryBlock().getFirstInsertionPt());
    IRB.CreateCall(AFLGoTraceFunDistance, {FunDistanceValue});
  }

  PreservedAnalyses PA;
  PA.preserve<AFLGoTargetDetectionAnalysis>();
  PA.preserve<AFLGoFunctionDistanceAnalysis>();
  return PreservedAnalyses::none();
}