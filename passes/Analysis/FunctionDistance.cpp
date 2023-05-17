#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/ADT/BreadthFirstIterator.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/Analysis/CallGraph.h>

#include "SVF-LLVM/LLVMModule.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "SVFIR/SVFIR.h"
#include "WPA/Andersen.h"

#include <memory>
#include <utility>

using namespace llvm;

static cl::opt<bool> ClExtendCG(
    "extend-cg",
    cl::desc("Extend call graph with indirect edges through pointer analysis"),
    cl::init(false));

namespace {

class InvertedCallGraphNode {
public:
  using CallRecord =
      std::pair<Optional<WeakTrackingVH>, InvertedCallGraphNode *>;

private:
  friend class InvertedCallGraph;

  const std::unique_ptr<CallGraphNode> &Inner;
  std::vector<CallRecord> Callers; // Non-owning pointers

public:
  using iterator = std::vector<CallRecord>::iterator;
  using const_iterator = std::vector<CallRecord>::const_iterator;

  inline iterator begin() { return Callers.begin(); }
  inline iterator end() { return Callers.end(); }
  inline const_iterator begin() const { return Callers.begin(); }
  inline const_iterator end() const { return Callers.end(); }

  InvertedCallGraphNode(const std::unique_ptr<CallGraphNode> &Inner)
      : Inner(Inner) {}

  const std::unique_ptr<CallGraphNode> &getInner() const { return Inner; }
};

class InvertedCallGraph {
  using FunctionMapTy =
      std::map<const Function *, std::unique_ptr<InvertedCallGraphNode>>;

  FunctionMapTy FunctionMap;

public:
  InvertedCallGraph(const CallGraph &CG) {
    // Construct all nodes
    for (auto &CGEntry : CG) {
      auto *F = CGEntry.first;
      auto &CGNode = CGEntry.second;
      FunctionMap[F] = std::make_unique<InvertedCallGraphNode>(CGNode);
    }

    // Fill caller references
    for (auto &CGEntry : CG) {
      auto *F = CGEntry.first;
      auto &CGNode = CGEntry.second;
      for (auto &CGNodePair : *CGNode) {
        auto &CallSite = CGNodePair.first;
        auto *Callee = CGNodePair.second;
        Function *CalleeF = Callee->getFunction();
        if (!CalleeF) {
          // Ignore external nodes
          continue;
        }

        FunctionMap[CalleeF]->Callers.push_back(
            std::make_pair(CallSite, FunctionMap[F].get()));
      }
    }
  }

  inline const InvertedCallGraphNode *operator[](const Function *F) const {
    const auto I = FunctionMap.find(F);
    return I->second.get();
  }
};

} // namespace

template <> struct GraphTraits<const InvertedCallGraphNode *> {
  using NodeRef = const InvertedCallGraphNode *;
  using CGNPairTy = InvertedCallGraphNode::CallRecord;
  using EdgeRef = const InvertedCallGraphNode::CallRecord &;

  static NodeRef getEntryNode(const InvertedCallGraphNode *StartICGNode) {
    return StartICGNode;
  }

  static NodeRef getValue(CGNPairTy P) { return P.second; }
  using ChildIteratorType =
      mapped_iterator<InvertedCallGraphNode::const_iterator,
                      decltype(&getValue)>;

  // NOLINTNEXTLINE(readability-identifier-naming)
  static ChildIteratorType child_begin(NodeRef N) {
    return ChildIteratorType(N->begin(), &getValue);
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  static ChildIteratorType child_end(NodeRef N) {
    return ChildIteratorType(N->end(), &getValue);
  }
};

void extendCallGraph(CallGraph &LLVMCallGraph, Module &M) {
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
}

std::map<Function *, unsigned int>
getDistancesFromFunction(Function &TargetFunction, InvertedCallGraph &ICG) {
  auto *TargetFunctionNode = ICG[&TargetFunction];

  std::map<Function *, unsigned int> DistancesFromTarget;
  for (auto BFIter = bf_begin(TargetFunctionNode);
       BFIter != bf_end(TargetFunctionNode); ++BFIter) {
    DistancesFromTarget[BFIter->getInner()->getFunction()] = BFIter.getLevel();
  }

  return DistancesFromTarget;
}

AnalysisKey AFLGoFunctionDistanceAnalysis::Key;

AFLGoFunctionDistanceAnalysis::Result
AFLGoFunctionDistanceAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {
  FunctionAnalysisManager &FAM =
      MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  std::unique_ptr<CallGraph> OwnedCG;
  std::unique_ptr<InvertedCallGraph> ICG;
  if (!ClExtendCG) {
    // If we are not extending the call graph, we can reuse the one provided by
    // the analysis.
    auto &CG = MAM.getResult<CallGraphAnalysis>(M);
    ICG = std::make_unique<InvertedCallGraph>(CG);
  } else {
    // We need to modify our own copy of the call graph to avoid breaking other
    // LLVM passes. This call graph is useful only to us anyway.
    OwnedCG = std::make_unique<CallGraph>(M);
    extendCallGraph(*OwnedCG, M);
    ICG = std::make_unique<InvertedCallGraph>(*OwnedCG);
  }

  std::map<Function *, std::vector<unsigned int>> DistancesFromTargets;
  for (auto &F : M) {
    auto BBTargets = FAM.getResult<AFLGoTargetDetectionAnalysis>(F);
    if (BBTargets.empty()) {
      // F is not a target.
      continue;
    }

    auto Distances = getDistancesFromFunction(F, *ICG);
    for (auto &DistanceEntry : Distances) {
      DistancesFromTargets[DistanceEntry.first].push_back(DistanceEntry.second);
    }
  }

  auto DistanceMap = DenseMap<Function *, double>();
  for (auto &DistancesFromTargetPair : DistancesFromTargets) {
    auto *Function = DistancesFromTargetPair.first;
    if (!Function)
      continue;

    auto &Distances = DistancesFromTargetPair.second;

    double HarmonicMean = 0;
    for (auto Distance : Distances) {
      HarmonicMean += 1.0 / Distance;
    }
    HarmonicMean = Distances.size() / HarmonicMean;

    DistanceMap[Function] = HarmonicMean;
  }

  return DistanceMap;
}