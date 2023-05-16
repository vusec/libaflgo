#include <AFLGoCompiler/TargetInjection.hpp>
#include <Analysis/TargetDetection.hpp>

#include <llvm/ADT/SmallString.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/VirtualFileSystem.h>

using namespace llvm;

cl::opt<std::string>
    TargetsPath("targets",
                cl::desc("Input file containing the target lines of code."),
                cl::value_desc("targets"));

bool AFLGoTargetInjectionPass::Target::matches(const DILocation &Loc) {
  auto Line = Loc.getLine();
  auto File = Loc.getFilename();
  auto Directory = Loc.getDirectory();

  if (File.empty()) {
    if (auto *OriginalLoc = Loc.getInlinedAt()) {
      Line = OriginalLoc->getLine();
      File = OriginalLoc->getFilename();
      Directory = OriginalLoc->getDirectory();
    }
  }

  auto AbsolutePath = SmallString<16>(File);
  sys::fs::make_absolute(Directory, AbsolutePath);
  auto RealPath = SmallString<16>();
  sys::fs::real_path(AbsolutePath, RealPath);

  return !RealPath.compare(this->File) && Line == this->Line;
}

void AFLGoTargetInjectionPass::parseTargets(
    std::unique_ptr<MemoryBuffer> &TargetsBuffer) {
  SmallVector<StringRef, 16> Lines;
  TargetsBuffer->getBuffer().split(Lines, '\n');
  for (auto Line : Lines) {
    if (Line.empty() || Line.starts_with("#")) {
      continue;
    }

    auto [File, LineNumStr] = Line.split(':');
    auto LineNum = std::stoul(LineNumStr.str());
    Targets.push_back(Target(File.str(), LineNum));
  }
}

AFLGoTargetInjectionPass::AFLGoTargetInjectionPass() {
  auto VFS = vfs::getRealFileSystem();
  auto BufferOrErr = VFS->getBufferForFile(TargetsPath);
  if (std::error_code EC = BufferOrErr.getError()) {
    std::string ErrorMessage = formatv("can't open targets file '{0}': {1}",
                                       TargetsPath, EC.message());
    report_fatal_error(Twine(ErrorMessage));
  }

  parseTargets(*BufferOrErr);
}

PreservedAnalyses AFLGoTargetInjectionPass::run(Module &M,
                                                ModuleAnalysisManager &MAM) {
  auto &C = M.getContext();
  auto *VoidTy = Type::getVoidTy(C);
  auto *Int32Ty = Type::getInt32Ty(C);
  auto AFLGoTraceBBTarget = M.getOrInsertFunction(
      AFLGoTargetDetectionAnalysis::TargetFunctionName, VoidTy, Int32Ty);

  for (auto &F : M) {
    if (F.isDeclaration()) {
      continue;
    }

    for (auto &BB : F) {
      bool BBIsTarget = false;

      for (auto &I : BB) {
        auto *Loc = I.getDebugLoc().get();
        if (!Loc) {
          continue;
        }

        for (auto &Target : Targets) {
          if (Target.matches(*Loc)) {
            BBIsTarget = true;
            break;
          }
        }

        if (BBIsTarget) {
          break;
        }
      }

      if (BBIsTarget) {
        IRBuilder<> IRB(&*BB.getFirstInsertionPt());
        auto *TargetIDPlaceholder = IRB.getInt32(0);
        IRB.CreateCall(AFLGoTraceBBTarget, {TargetIDPlaceholder});
      }
    }
  }

  return PreservedAnalyses::none();
}