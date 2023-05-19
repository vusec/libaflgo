#include <AFLGoLinker/DistanceInstrumentation.hpp>
#include <AFLGoLinker/FunctionDistanceInstrumentation.hpp>
#include <AFLGoLinker/TargetInjectionFixup.hpp>

#include <Analysis/BasicBlockDistance.hpp>
#include <Analysis/ExtendedCallGraph.hpp>
#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

using namespace llvm;

static cl::opt<bool> ClExtendCG(
    "extend-cg",
    cl::desc("Extend call graph with indirect edges through pointer analysis"),
    cl::init(false));

static cl::opt<bool>
    ClHawkeyeDistance("use-hawkeye-distance",
                      cl::desc("Use Hawkeye function distance definition"),
                      cl::init(false));

static cl::opt<bool>
    ClTraceFunctionDistance("trace-function-distance",
                            cl::desc("Add function distance tracing callbacks"),
                            cl::init(false));

llvm::PassPluginLibraryInfo getAFLGoLinkerPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "AFLGoLinker", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerAnalysisRegistrationCallback(
            [](FunctionAnalysisManager &FAM) {
              FAM.registerPass([] { return AFLGoTargetDetectionAnalysis(); });
            });
        PB.registerAnalysisRegistrationCallback([](ModuleAnalysisManager &MAM) {
          MAM.registerPass([] { return ExtendedCallGraphAnalysis(); });
          MAM.registerPass([] {
            return AFLGoFunctionDistanceAnalysis(ClExtendCG, ClHawkeyeDistance);
          });
          MAM.registerPass(
              [] { return AFLGoBasicBlockDistanceAnalysis(ClExtendCG); });
        });

        PB.registerFullLinkTimeOptimizationLastEPCallback(
            [](ModulePassManager &MPM, OptimizationLevel) {
              if (ClTraceFunctionDistance) {
                MPM.addPass(FunctionDistancePass());
              }
              MPM.addPass(AFLGoDistanceInstrumentationPass());
              MPM.addPass(AFLGoTargetInjectionFixupPass());
            });

        PB.registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "instrument-linker-aflgo") {
                if (ClTraceFunctionDistance) {
                  MPM.addPass(FunctionDistancePass());
                }
                MPM.addPass(AFLGoDistanceInstrumentationPass());
                MPM.addPass(AFLGoTargetInjectionFixupPass());
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