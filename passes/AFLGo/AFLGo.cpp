#include <Analysis/BasicBlockDistance.hpp>
#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDefinition.hpp>

#include <llvm/IR/IRBuilder.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

using namespace llvm;

namespace {

const char *AFLGoTraceBBDistanceName = "__aflgo_trace_bb_distance";

class AFLGoPass : public PassInfoMixin<AFLGoPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
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

  static bool isRequired() { return true; }
};

llvm::PassPluginLibraryInfo getAFLGoPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "AFLGo", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerAnalysisRegistrationCallback(
            [](FunctionAnalysisManager &FAM) {
              FAM.registerPass([] { return AFLGoTargetDefinitionAnalysis(); });
            });
        PB.registerAnalysisRegistrationCallback([](ModuleAnalysisManager &MAM) {
          MAM.registerPass([] { return AFLGoFunctionDistanceAnalysis(); });
        });
        PB.registerAnalysisRegistrationCallback([](ModuleAnalysisManager &MAM) {
          MAM.registerPass([] { return AFLGoBasicBlockDistanceAnalysis(); });
        });

        PB.registerFullLinkTimeOptimizationLastEPCallback(
            [](ModulePassManager &MPM, OptimizationLevel) {
              MPM.addPass(AFLGoPass());
            });

        PB.registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "instrument-aflgo") {
                MPM.addPass(AFLGoPass());
                return true;
              }

              return false;
            });
      }};
}

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getAFLGoPluginInfo();
}