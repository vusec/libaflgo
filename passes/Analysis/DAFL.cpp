#include <Analysis/DAFL.hpp>
#include <Analysis/TargetDetection.hpp>

// #include "AbstractExecution/SVFIR2ItvExeState.h"
#include "Graphs/IRGraph.h"
#include "Graphs/VFG.h"
#include "Graphs/VFGEdge.h"
#include "Graphs/VFGNode.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/Andersen.h"

#include <llvm/ADT/SmallSet.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

AnalysisKey DAFLAnalysis::Key;

typedef long long NodeID;

struct Edge {
  const NodeID Target;
  const DAFLAnalysis::WeightTy Weight;
};

DAFLAnalysis::Result
DAFLAnalysis::readFromFile(Module &M, std::unique_ptr<MemoryBuffer> &Buffer) {
  auto Res = std::make_unique<Result::element_type>();
  SmallVector<StringRef, 16> Lines;
  Buffer->getBuffer().split(Lines, '\n');
  for (auto Line : Lines) {
    if (Line.empty() || Line[0] == '#') {
      continue;
    }

    SmallVector<StringRef, 3> LineSplit;
    Line.split(LineSplit, ',');
    if (LineSplit.size() != 3) {
      auto Err = formatv("Invalid DAFL input file format: got {0} columns",
                         LineSplit.size());
      report_fatal_error(Err);
    }

    auto Score = std::stoull(LineSplit[0].str());
    auto FnName = LineSplit[1];

    auto *F = M.getFunction(FnName);
    if (!F || F->isDeclaration()) {
      auto Err = formatv("Function '{0}' not found in module", FnName);
      report_fatal_error(Err);
    }

    auto FileLine = LineSplit[2].split(':');
    auto FilePath = FileLine.first;
    auto LineNum = std::stoi(FileLine.second.str());

    auto FoundBB = false;
    for (auto &I : instructions(*F)) {
      auto *Loc = I.getDebugLoc().get();
      if (!Loc || Loc->getFilename().empty() || Loc->getLine() == 0) {
        continue;
      }

      auto AbsolutePath = SmallString<32>(Loc->getFilename());
      sys::fs::make_absolute(Loc->getDirectory(), AbsolutePath);
      auto RealPath = SmallString<32>();
      sys::fs::real_path(AbsolutePath, RealPath);

      if (FilePath.compare(RealPath) == 0 && LineNum == Loc->getLine()) {
        auto *BB = I.getParent();
        auto *R = Res.get();
        if (R->find(BB) == R->end()) {
          R->insert({BB, Score});
        } else {
          auto Err =
              formatv("Duplicate score for basic block '{0}' in function '{1}'",
                      BB->getName(), FnName);
          report_fatal_error(Err);
        }

        FoundBB = true;
        break;
      }
    }

    if (!FoundBB) {
      auto Err = formatv("Basic block not found in function '{0}': {1}:{2}",
                         FnName, FilePath, LineNum);
      report_fatal_error(Err);
    }
  }
  return Res;
}

DAFLAnalysis::Result DAFLAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {

  if (!InputFile.empty()) {
    auto VFS = vfs::getRealFileSystem();
    auto BufferOrErr = VFS->getBufferForFile(InputFile);
    if (auto EC = BufferOrErr.getError()) {
      auto ErrorMessage = formatv("can't open DAFL input file '{0}': {1}",
                                  InputFile, EC.message());
      report_fatal_error(ErrorMessage);
    }

    return readFromFile(M, *BufferOrErr);
  }

  auto *LLVMModuleSet = SVF::LLVMModuleSet::getLLVMModuleSet();
  auto *SVFModule = LLVMModuleSet->buildSVFModule(M);

  if (DebugFiles) {
    LLVMModuleSet->dumpModulesToFile(".svf.bc");
  }

  SVF::SVFIRBuilder Builder(SVFModule);
  auto *PAG = Builder.build();

  if (DebugFiles) {
    PAG->dump("pag");
  }

  auto *Andersen = SVF::AndersenWaveDiff::createAndersenWaveDiff(PAG);

  /// Call Graph
  // SVF::PTACallGraph *Callgraph = Andersen->getPTACallGraph();

  /// Value-Flow Graph (VFG)
  // SVF::VFG *VFG = new SVF::VFG(Callgraph);

  /// Sparse value-flow graph (SVFG)
  SVF::SVFGBuilder SvfBuilder(true);
  SVF::SVFG *SVFG = SvfBuilder.buildFullSVFG(Andersen);
  // updateCallGraph() is called in buildFullSVFG()->build() if true is passed
  // to SVFGBuilder constructor

  if (DebugFiles) {
    SVFG->dump("svfg");
  }

  // get the target instructions
  SmallSet<const Instruction *, 32> TargetIs;
  auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  for (auto &F : M) {
    auto &FTargets = FAM.getResult<AFLGoTargetDetectionAnalysis>(F);
    TargetIs.insert(FTargets.Is.begin(), FTargets.Is.end());
  }

  // If the target is a call instruction, with optimization enabled, it may
  // happen that it is the only instruction that corresponds to the target line
  // number (i.e. it's the only instruction in TargetIs). At the same time the
  // SVFG does not contain nodes for call instructions. In this case it's safe
  // to add the arguments to the call to the target instructions set.
  SmallVector<const Instruction *, 32> TargetsToAdd;
  SmallVector<const Instruction *, 32> TargetsToRemove;
  for (const auto *I : TargetIs) {
    if (const auto *CB = dyn_cast<CallBase>(I)) {
      TargetsToRemove.push_back(I);
      for (const auto &Arg : CB->args()) {
        if (const auto *ArgI = dyn_cast<Instruction>(Arg)) {
          TargetsToAdd.push_back(ArgI);
        }
      }
    }
  }

  for (const auto *I : TargetsToAdd) {
    TargetIs.insert(I);
  }
  for (const auto *I : TargetsToRemove) {
    TargetIs.erase(I);
  }

  // skip intrinsics and instructions that are not in SVFG
  TargetsToRemove.clear();
  for (const auto *I : TargetIs) {
    errs() << "AAAAA " << *I << "\n";
    if (auto *II = dyn_cast<IntrinsicInst>(I)) {
      errs() << "Skipping intrinsic target inst: " << *I << "\n";
      TargetsToRemove.push_back(I);
    } else if (auto *BI = dyn_cast<BranchInst>(I)) {
      errs() << "Skipping branch target inst: " << *I << "\n";
      TargetsToRemove.push_back(I);
    }
  }

  for (const auto *I : TargetsToRemove) {
    TargetIs.erase(I);
  }

  // track which instructions are present in SVFG
  decltype(TargetIs) SeenTargetIs;

  // adjacency list from node to its defs
  std::map<NodeID, std::vector<Edge>> G;
  // use a dummy node to connect to target instructions
  auto SentinelNode = -1;

  for (auto SVFGNodeIt = SVFG->begin(), SVFGNodeItEnd = SVFG->end();
       SVFGNodeIt != SVFGNodeItEnd; ++SVFGNodeIt) {
    auto *Node = SVFGNodeIt->second;

    // FileLine NodeLoc;
    // auto NodeHasLoc = getDebugFileLine(Node, NodeLoc);
    auto *NodeSVFVal = Node->getValue();
    auto *NodeVal =
        NodeSVFVal ? LLVMModuleSet->getLLVMValue(NodeSVFVal) : nullptr;
    auto *NodeInst = dyn_cast_or_null<Instruction>(NodeVal);

    if (NodeInst) {
      for (auto *TargetI : TargetIs) {
        if (NodeInst == TargetI) {
          SeenTargetIs.insert(NodeInst);
          // connect sentinel node to target node
          G[SentinelNode].push_back({Node->getId(), 0});
        }
      }
    }

    G[Node->getId()] = std::vector<Edge>();

    for (auto InEdgeIt = Node->InEdgeBegin(), InEdgeItEnd = Node->InEdgeEnd();
         InEdgeIt != InEdgeItEnd; ++InEdgeIt) {
      auto *Edge = *InEdgeIt;
      auto *DefNode = Edge->getSrcNode();

      WeightTy Weight = 1;
      if (!NodeInst) {
        Weight = 0;
      }

      G[Node->getId()].push_back({DefNode->getId(), Weight});
    }
  }

  auto HasAllTargets = true;
  for (auto *TargetI : TargetIs) {
    if (SeenTargetIs.find(TargetI) == SeenTargetIs.end()) {
      HasAllTargets = false;
      errs() << "Target not found in SVFG: " << *TargetI << "\n";
    }
  }

  if (!HasAllTargets) {
    report_fatal_error("Not all targets found in SVFG");
  }

  // compute distance from sentinel node to all other nodes
  std::map<NodeID, WeightTy> Dist;
  std::map<NodeID, NodeID> Pred;
  auto UnreachableDist = std::numeric_limits<WeightTy>::max();
  for (auto &KV : G) {
    Dist[KV.first] = UnreachableDist;
    Pred[KV.first] = -1;
  }

  Dist[SentinelNode] = 0;
  std::set<std::pair<WeightTy, NodeID>> Q;

  Q.insert({Dist[SentinelNode], SentinelNode});

  while (!Q.empty()) {
    auto It = Q.begin();
    auto D = It->first;
    auto U = It->second;
    Q.erase(It);

    for (auto &Edge : G[U]) {
      auto V = Edge.Target;
      auto W = Edge.Weight;
      if (D + W < Dist[V]) {
        Q.erase({Dist[V], V});
        Dist[V] = D + W;
        Pred[V] = U;
        Q.insert({Dist[V], V});
      }
    }
  }

  WeightTy MaxDist = 0;
  for (auto &KV : Dist) {
    if (KV.first == SentinelNode || KV.second == UnreachableDist) {
      continue;
    }
    MaxDist = std::max(MaxDist, KV.second);
  }

  Result Res = std::make_unique<Result::element_type>();
  for (auto &KV : Dist) {
    if (KV.first == SentinelNode || KV.second == UnreachableDist) {
      continue;
    }

    auto *Node = SVFG->getVFGNode(KV.first);
    auto *SVFVal = Node->getValue();
    auto *LLVMVal = SVFVal ? LLVMModuleSet->getLLVMValue(SVFVal) : nullptr;
    if (auto *I = dyn_cast_or_null<Instruction>(LLVMVal)) {
      // score is proximity to target; higher is better
      auto Score = MaxDist - KV.second;
      auto *BB = I->getParent();
      auto *R = Res.get();
      if (R->find(BB) == R->end()) {
        R->insert({BB, Score});
      } else {
        (*R)[BB] = std::max(Score, (*R)[BB]);
      }
    }
  }

  // clean up memory
  SVF::AndersenWaveDiff::releaseAndersenWaveDiff();
  SVF::SVFIR::releaseSVFIR();

  SVF::LLVMModuleSet::releaseLLVMModuleSet();

  return Res;
}