#include <Analysis/BasicBlockDistance.hpp>
#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/Analysis/CallGraph.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

using namespace llvm;

namespace {

class AFLGoFunctionDistancePrinterPass
    : public PassInfoMixin<AFLGoFunctionDistancePrinterPass> {
  raw_ostream &OS;

public:
  explicit AFLGoFunctionDistancePrinterPass(raw_ostream &OS) : OS(OS) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    auto &FunctionDistances = AM.getResult<AFLGoFunctionDistanceAnalysis>(M);

    OS << "function_name,distance\n";
    for (const auto &[Function, Distance] : FunctionDistances) {
      OS << formatv("{0},{1}\n", Function->getName(), Distance);
    }

    return PreservedAnalyses::all();
  }
};

class AFLGoTargetDetectionPrinterPass
    : public PassInfoMixin<AFLGoTargetDetectionPrinterPass> {
  raw_ostream &OS;

public:
  explicit AFLGoTargetDetectionPrinterPass(raw_ostream &OS) : OS(OS) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    FunctionAnalysisManager &FAM =
        MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    OS << "function_name,target_count\n";
    for (auto &F : M) {
      auto Targets = FAM.getResult<AFLGoTargetDetectionAnalysis>(F);
      if (Targets.size()) {
        OS << formatv("{0},{1}\n", F.getName(), Targets.size());
      }
    }

    return PreservedAnalyses::all();
  }
};

class AFLGoBasicBlockDistancePrinterPass
    : public PassInfoMixin<AFLGoBasicBlockDistancePrinterPass> {
  raw_ostream &OS;

public:
  explicit AFLGoBasicBlockDistancePrinterPass(raw_ostream &OS) : OS(OS) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    auto BBDistanceResult = MAM.getResult<AFLGoBasicBlockDistanceAnalysis>(M);

    OS << "function_name,basic_block_name,distance\n";
    for (auto &F : M) {
      auto DistanceMap = BBDistanceResult.computeBBDistances(F);
      for (auto [BB, Distance] : DistanceMap) {
        OS << formatv("{0},", F.getName());
        BB->printAsOperand(OS, false);
        OS << formatv(",{0}\n", Distance);
      }
    }

    return PreservedAnalyses::all();
  }
};

llvm::PassPluginLibraryInfo getAFLGoAnalysisPrinterPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "AFLGoAnalysisPrinter", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerAnalysisRegistrationCallback(
            [](FunctionAnalysisManager &FAM) {
              FAM.registerPass([] { return AFLGoTargetDetectionAnalysis(); });
            });

        PB.registerAnalysisRegistrationCallback([](ModuleAnalysisManager &MAM) {
          MAM.registerPass([] { return AFLGoFunctionDistanceAnalysis(); });
        });
        PB.registerAnalysisRegistrationCallback([](ModuleAnalysisManager &MAM) {
          MAM.registerPass([] { return AFLGoBasicBlockDistanceAnalysis(); });
        });

        PB.registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "print-aflgo-target-detection") {
                MPM.addPass(AFLGoTargetDetectionPrinterPass(dbgs()));
                return true;
              }

              if (Name == "print-aflgo-function-distance") {
                MPM.addPass(AFLGoFunctionDistancePrinterPass(dbgs()));
                return true;
              }

              if (Name == "print-aflgo-basic-block-distance") {
                MPM.addPass(AFLGoBasicBlockDistancePrinterPass(dbgs()));
                return true;
              }

              return false;
            });
      }};
}

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getAFLGoAnalysisPrinterPluginInfo();
}