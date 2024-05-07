#pragma once

#include <llvm/IR/PassManager.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/Support/MemoryBuffer.h>

#include <memory>

namespace llvm {

class DAFLAnalysis : public AnalysisInfoMixin<DAFLAnalysis> {
public:
  static AnalysisKey Key;

  using WeightTy = uint64_t;
  using Result = std::unique_ptr<ValueMap<const BasicBlock *, WeightTy>>;

  DAFLAnalysis(std::string InputFile) : InputFile(InputFile) {}

  Result run(Module &M, ModuleAnalysisManager &);

private:
  Result readFromFile(Module &, std::unique_ptr<MemoryBuffer> &);

  std::string InputFile;
};

} // namespace llvm