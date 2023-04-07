#include <Analysis/TargetDefinition.hpp>

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/VirtualFileSystem.h>

using namespace llvm;

cl::opt<std::string>
    TargetsPath("targets",
                cl::desc("Input file containing the target lines of code."),
                cl::value_desc("targets"));

bool AFLGoTargetDefinitionAnalysis::Target::matches(const DILocation &Loc) {
  auto Line = Loc.getLine();
  auto File = Loc.getFilename();

  if (File.empty()) {
    if (auto *OriginalLoc = Loc.getInlinedAt()) {
      Line = OriginalLoc->getLine();
      File = OriginalLoc->getFilename();
    }
  }

  return !this->File.compare(File) && this->Line == Line;
}

AnalysisKey AFLGoTargetDefinitionAnalysis::Key;

void AFLGoTargetDefinitionAnalysis::parseTargets(
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

AFLGoTargetDefinitionAnalysis::AFLGoTargetDefinitionAnalysis() {
  auto VFS = vfs::getRealFileSystem();
  auto BufferOrErr = VFS->getBufferForFile(TargetsPath);
  if (std::error_code EC = BufferOrErr.getError()) {
    std::string ErrorMessage = formatv("can't open targets file '{0}': {1}",
                                       TargetsPath, EC.message());
    report_fatal_error(Twine(ErrorMessage));
  }

  parseTargets(*BufferOrErr);
}

AFLGoTargetDefinitionAnalysis::Result
AFLGoTargetDefinitionAnalysis::run(Function &F, FunctionAnalysisManager &FAM) {
  SmallSet<BasicBlock *, 16> BBTargets;
  for (auto &BB : F) {
    bool BBIsTarget = false;

    for (auto &I : BB) {
      auto *Loc = I.getDebugLoc().get();
      if (!Loc) {
        continue;
      }

      for (auto &Target : Targets) {
        if (Target.matches(*Loc)) {
          BBTargets.insert(&BB);
          BBIsTarget = true;
          break;
        }
      }

      if (BBIsTarget) {
        break;
      }
    }
  }

  return BBTargets;
}
