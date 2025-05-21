#ifndef LLVM_TRANSFORMS_SCHED_H
#define LLVM_TRANSFORMS_SCHED_H

#include "llvm/IR/PassManager.h"
namespace llvm {

class SchedPass : public PassInfoMixin<SchedPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_SCHED_H
