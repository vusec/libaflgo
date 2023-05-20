#include <Analysis/ExtendedCallGraph.hpp>

#include "SVF-LLVM/LLVMModule.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"

#include <memory>
#include <utility>

using namespace llvm;

AnalysisKey ExtendedCallGraphAnalysis::Key;

ExtendedCallGraphAnalysis::Result
ExtendedCallGraphAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {
  // There is no other way to disable printing inside SVF.
  auto &PrintOption = const_cast<Option<bool> &>(SVF::Options::PStat);
  PrintOption.setValue(false);

  // We need to modify our own copy of the call graph to avoid breaking other
  // LLVM passes. This call graph is useful only to us anyway.
  auto LLVMCallGraph = CallGraph(M);

  auto *LLVMModuleSet = SVF::LLVMModuleSet::getLLVMModuleSet();
  auto *SVFModule = LLVMModuleSet->buildSVFModule(M);

  SVF::SVFIRBuilder Builder(SVFModule);
  auto *PAG = Builder.build();

  auto *Andersen = SVF::AndersenWaveDiff::createAndersenWaveDiff(PAG);
  auto *SVFCallGraph = Andersen->getPTACallGraph();

  auto &IndCallMap = SVFCallGraph->getIndCallMap();
  for (auto &IndCallEntry : IndCallMap) {
    auto *SVFCallNode = IndCallEntry.first;
    auto *SVFCaller = SVFCallNode->getCaller();
    auto *LLVMCaller = cast<Function>(LLVMModuleSet->getLLVMValue(SVFCaller));
    auto *LLVMCallerNode = LLVMCallGraph[LLVMCaller];

    auto &Callees = IndCallEntry.second;
    for (auto *SVFCallee : Callees) {
      auto *LLVMCallee = cast<Function>(LLVMModuleSet->getLLVMValue(SVFCallee));
      auto *SVFCall = SVFCallNode->getCallSite();
      auto *LLVMCall = cast<CallBase>(LLVMModuleSet->getLLVMValue(SVFCall));
      LLVMCallerNode->addCalledFunction(const_cast<CallBase *>(LLVMCall),
                                        LLVMCallGraph[LLVMCallee]);
    }
  }

  SVF::AndersenWaveDiff::releaseAndersenWaveDiff();
  SVF::SVFIR::releaseSVFIR();
  SVF::LLVMModuleSet::releaseLLVMModuleSet();

  return LLVMCallGraph;
}