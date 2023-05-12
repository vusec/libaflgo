#include <AFLGoLinker/DistanceInstrumentation.hpp>
#include <AFLGoLinker/TargetInjectionFixup.hpp>

#include <Analysis/BasicBlockDistance.hpp>
#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

using namespace llvm;

llvm::PassPluginLibraryInfo getAFLGoLinkerPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "AFLGoLinker", LLVM_VERSION_STRING,
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

        PB.registerFullLinkTimeOptimizationLastEPCallback(
            [](ModulePassManager &MPM, OptimizationLevel) {
              MPM.addPass(AFLGoTargetInjectionFixupPass());
              MPM.addPass(AFLGoDistanceInstrumentationPass());
            });

        PB.registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "instrument-linker-aflgo") {
                MPM.addPass(AFLGoTargetInjectionFixupPass());
                MPM.addPass(AFLGoDistanceInstrumentationPass());
                return true;
              }

              return false;
            });
      }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getAFLGoLinkerPluginInfo();
}