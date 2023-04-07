#pragma once

#include <llvm/ADT/SmallSet.h>
#include <llvm/IR/PassManager.h>

namespace llvm {

class AFLGoTargetDefinitionAnalysis
    : public AnalysisInfoMixin<AFLGoTargetDefinitionAnalysis> {

  class Target {
    std::string File;
    unsigned int Line;

  public:
    Target(std::string File, unsigned int Line) : File(File), Line(Line) {}

    bool matches(const DILocation &Loc);
  };

  SmallVector<Target, 16> Targets;
  void parseTargets(std::unique_ptr<MemoryBuffer> &TargetsBuffer);

public:
  using Result = SmallSet<BasicBlock *, 16>;

  static AnalysisKey Key;

  AFLGoTargetDefinitionAnalysis();

  Result run(Function &F, FunctionAnalysisManager &FAM);
};

} // namespace llvm