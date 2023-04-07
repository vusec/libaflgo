#include "Analysis/TargetDefinition.hpp"
#include <Analysis/FunctionDistance.hpp>

#include <llvm/ADT/BreadthFirstIterator.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/Analysis/CallGraph.h>
#include <memory>

using namespace llvm;

namespace {

class InvertedCallGraphNode {
  friend class InvertedCallGraph;

  const std::unique_ptr<CallGraphNode> &Inner;
  std::vector<InvertedCallGraphNode *> Callers; // Non-owning pointers

public:
  using iterator = std::vector<InvertedCallGraphNode *>::iterator;
  using const_iterator = std::vector<InvertedCallGraphNode *>::const_iterator;

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
    for (auto &[F, CGNode] : CG) {
      FunctionMap[F] = std::make_unique<InvertedCallGraphNode>(CGNode);
    }

    // Fill caller references
    for (auto &[F, CGNode] : CG) {
      for (auto [CallSite, Callee] : *CGNode) {
        Function *CalleeF = Callee->getFunction();
        if (!CalleeF) {
          // Ignore external nodes
          continue;
        }

        FunctionMap[CalleeF]->Callers.push_back(FunctionMap[F].get());
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
  using ChildIteratorType = InvertedCallGraphNode::const_iterator;

  static NodeRef getEntryNode(const InvertedCallGraphNode *StartICGNode) {
    return StartICGNode;
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  static ChildIteratorType child_begin(NodeRef N) {
    return ChildIteratorType(N->begin());
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  static ChildIteratorType child_end(NodeRef N) {
    return ChildIteratorType(N->end());
  }
};

AnalysisKey AFLGoFunctionDistanceAnalysis::Key;

AFLGoFunctionDistanceAnalysis::Result
AFLGoFunctionDistanceAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {
  FunctionAnalysisManager &FAM =
      MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  auto &CG = MAM.getResult<CallGraphAnalysis>(M);
  auto ICG = InvertedCallGraph{CG};

  auto TargetFunctionNodes = std::vector<const InvertedCallGraphNode *>();
  for (auto &F : M) {
    auto BBTargets = FAM.getResult<AFLGoTargetDefinitionAnalysis>(F);
    if (!BBTargets.empty()) {
      TargetFunctionNodes.push_back(ICG[&F]);
    }
  }

  std::map<Function *, std::vector<unsigned int>> DistancesFromTargets;
  for (auto *TargetFunctionNode : TargetFunctionNodes) {
    for (auto BFIter = bf_begin(TargetFunctionNode);
         BFIter != bf_end(TargetFunctionNode); ++BFIter) {
      DistancesFromTargets[BFIter->getInner()->getFunction()].push_back(
          BFIter.getLevel());
    }
  }

  auto DistanceMap = DenseMap<Function *, double>();
  for (auto [Function, Distances] : DistancesFromTargets) {
    if (!Function)
      continue;

    double HarmonicMean = 0;
    for (auto Distance : Distances) {
      HarmonicMean += 1.0 / Distance;
    }
    HarmonicMean = Distances.size() / HarmonicMean;

    DistanceMap[Function] = HarmonicMean;
  }

  return DistanceMap;
}