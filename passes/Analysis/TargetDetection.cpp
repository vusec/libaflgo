#include <Analysis/TargetDetection.hpp>

#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

AnalysisKey AFLGoTargetDetectionAnalysis::Key;

static bool hasAnnotation(const Instruction &I, const char *Annotation) {
  auto *Existing = I.getMetadata(LLVMContext::MD_annotation);
  if (Existing) {
    auto *Tuple = cast<MDTuple>(Existing);
    for (auto &N : Tuple->operands()) {
      if (cast<MDString>(N)->getString().equals(Annotation)) {
        return true;
      }
    }
  }
  return false;
}

AFLGoTargetDetectionAnalysis::Result
AFLGoTargetDetectionAnalysis::run(Function &F, FunctionAnalysisManager &FAM) {
  Result Targets;

  for (auto &BB : F) {
    auto IsBBTarget = false;
    auto HasTargetInstr = false;

    for (auto &I : BB) {

      if (auto *CB = dyn_cast<CallBase>(&I)) {
        auto *Callee = CB->getCalledFunction();
        if (!Callee) {
          continue;
        }

        if (Callee->getName().equals(TargetFunctionName)) {
          if (IsBBTarget) {
            auto Err = formatv("Multiple target calls in BB:\n{0}", BB);
            report_fatal_error(Twine(Err));
          }
          Targets.BBs.push_back({&BB, CB});
          IsBBTarget = true;
        }
      }

      if (hasAnnotation(I, TargetInstructionAnnotation)) {
        Targets.Is.push_back(&I);
        HasTargetInstr = true;
      }
    }

    if (IsBBTarget && !HasTargetInstr) {
      auto Err = formatv("Target BB without target instructions:\n{0}", BB);
      report_fatal_error(Twine(Err));
    }
  }

  return Targets;
}
