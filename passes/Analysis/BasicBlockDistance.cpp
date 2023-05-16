#include <Analysis/BasicBlockDistance.hpp>
#include <Analysis/FunctionDistance.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/ADT/BreadthFirstIterator.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

const double FunctionDistanceMagnificationFactor = 10;

AnalysisKey AFLGoBasicBlockDistanceAnalysis::Key;

AFLGoBasicBlockDistanceAnalysis::Result
AFLGoBasicBlockDistanceAnalysis::run(Module &M, ModuleAnalysisManager &MAM) {
  AFLGoBasicBlockDistanceAnalysis::Result::FunctionToOriginBBsMapTy
      FunctionToOriginBBs;

  auto &CG = MAM.getResult<CallGraphAnalysis>(M);
  auto &FunctionDistances = MAM.getResult<AFLGoFunctionDistanceAnalysis>(M);
  FunctionAnalysisManager &FAM =
      MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  for (Function &F : M) {
    if (F.isDeclaration()) {
      continue;
    }

    SmallDenseMap<BasicBlock *, double, 16> OriginBBs;

    auto &TargetBBs = FAM.getResult<AFLGoTargetDetectionAnalysis>(F);
    for (auto &TargetBBPair : TargetBBs) {
      auto *TargetBB = TargetBBPair.first;
      OriginBBs[TargetBB] = 0;
    }

    auto *CGNode = CG[&F];
    for (auto &CallEdge : *CGNode) {
      auto &CallInstOpt = CallEdge.first;
      if (!CallInstOpt) {
        continue;
      }

      CallBase *CallInst = cast<CallBase>(*CallInstOpt);
      auto *CalledFunction = CallInst->getCalledFunction();
      if (FunctionDistances.find(CalledFunction) == FunctionDistances.end()) {
        continue;
      }
      auto CallBBDistance = (FunctionDistances[CalledFunction] + 1) *
                            FunctionDistanceMagnificationFactor;

      auto *CallBB = CallInst->getParent();
      if (OriginBBs.find(CallBB) != OriginBBs.end()) {
        // When multiple calls appear in the same basic block, keep the one that
        // generates the minimum distance.
        OriginBBs[CallBB] = std::min(OriginBBs[CallBB], CallBBDistance);
      } else {
        OriginBBs[CallBB] = CallBBDistance;
      }
    }

    FunctionToOriginBBs[&F] = OriginBBs;
  }

  return Result{FunctionToOriginBBs, FunctionDistances};
}

AFLGoBasicBlockDistanceAnalysis::Result::BBToDistanceTy
AFLGoBasicBlockDistanceAnalysis::Result::computeBBDistances(Function &F) {
  auto OriginBBs = FunctionToOriginBBs[&F];

  auto DistanceMap = AFLGoBasicBlockDistanceAnalysis::Result::BBToDistanceTy();
  std::map<BasicBlock *, std::vector<double>> DistancesFromOrigins;
  for (auto &OriginBBPair : OriginBBs) {
    auto *OriginBB = OriginBBPair.first;
    auto OriginBBDistance = OriginBBPair.second;
    DistanceMap[OriginBB] = OriginBBDistance;

    auto InverseOriginBB = static_cast<Inverse<BasicBlock *>>(OriginBB);
    for (auto BFIter = bf_begin(InverseOriginBB);
         BFIter != bf_end(InverseOriginBB); ++BFIter) {
      if (OriginBBs.find(*BFIter) != OriginBBs.end()) {
        // This basic block is either a target or performs an external call.
        continue;
      }

      DistancesFromOrigins[*BFIter].push_back(OriginBBDistance +
                                              BFIter.getLevel());
    }
  }

  for (auto &DistancesFromOriginPair : DistancesFromOrigins) {
    auto &Distances = DistancesFromOriginPair.second;

    double HarmonicMean = 0;
    for (auto Distance : Distances) {
      HarmonicMean += 1.0 / Distance;
    }
    HarmonicMean = Distances.size() / HarmonicMean;

    auto *BB = DistancesFromOriginPair.first;
    DistanceMap[BB] = HarmonicMean;
  }

  return DistanceMap;
}