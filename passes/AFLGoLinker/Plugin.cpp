#include <AFLGoLinker/DAFL.hpp>
#include <AFLGoLinker/DistanceInstrumentation.hpp>
#include <AFLGoLinker/FunctionDistanceInstrumentation.hpp>
#include <AFLGoLinker/TargetInjectionFixup.hpp>

#include <Analysis/BasicBlockDistance.hpp>
#include <Analysis/DAFL.hpp>
#include <Analysis/ExtendedCallGraph.hpp>
#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Instrumentation.h>
#include <llvm/Transforms/Instrumentation/SanitizerCoverage.h>

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

static cl::opt<bool> ClDAFL("dafl", cl::desc("Enable DAFL instrumentation"),
                            cl::init(false));

static cl::opt<bool>
    ClDAFLDebug("dafl-debug",
                cl::desc("Save debug files for DAFL instrumentation"),
                cl::init(false));

static cl::opt<std::string>
    ClDAFLInputFile("dafl-input-file",
                    cl::desc("Input file for DAFL analysis results"),
                    cl::value_desc("filename"));

static cl::opt<std::string>
    ClDAFLOutputFile("dafl-output-file",
                     cl::desc("Output file for DAFL analysis results"),
                     cl::value_desc("filename"));

static void addPasses(ModulePassManager &MPM) {
  if (ClDAFL) {
    MPM.addPass(DAFLInstrumentationPass(ClDAFLOutputFile));
  } else {
    if (ClTraceFunctionDistance) {
      MPM.addPass(FunctionDistancePass());
    }
    MPM.addPass(AFLGoDistanceInstrumentationPass());
  }

  SanitizerCoverageOptions Options;
  Options.CoverageType = SanitizerCoverageOptions::SCK_Edge;
  Options.TracePCGuard = true;
  Options.TraceCmp = true;
  MPM.addPass(ModuleSanitizerCoveragePass(Options));

  MPM.addPass(AFLGoTargetInjectionFixupPass());
}

llvm::PassPluginLibraryInfo getAFLGoLinkerPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "AFLGoLinker", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerAnalysisRegistrationCallback(
            [](FunctionAnalysisManager &FAM) {
              FAM.registerPass([] { return AFLGoTargetDetectionAnalysis(); });
            });
        PB.registerAnalysisRegistrationCallback([](ModuleAnalysisManager &MAM) {
          MAM.registerPass(
              [] { return DAFLAnalysis(ClDAFLInputFile, ClDAFLDebug); });
          MAM.registerPass([] { return ExtendedCallGraphAnalysis(); });
          MAM.registerPass([] {
            return AFLGoFunctionDistanceAnalysis(ClExtendCG, ClHawkeyeDistance);
          });
          MAM.registerPass(
              [] { return AFLGoBasicBlockDistanceAnalysis(ClExtendCG); });
        });

        PB.registerFullLinkTimeOptimizationLastEPCallback(
            [](ModulePassManager &MPM, OptimizationLevel) { addPasses(MPM); });

        PB.registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "instrument-linker-aflgo") {
                addPasses(MPM);
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