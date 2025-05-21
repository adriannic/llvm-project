#include "llvm/Transforms/Utils/Sched.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"

using namespace llvm;

static CallInst *getAcquireCall(BasicBlock &Block) {
  for (Instruction &Instr : Block) {
    if (CallInst *Call = dyn_cast<CallInst>(&Instr)) {
      if (Function *Func = Call->getCalledFunction()) {
        if (Func->getName().contains("acquire")) {
          return Call;
        }
      }
    }
  }
  return nullptr;
}

static GetElementPtrInst *getGEP(BasicBlock &Block,
                                 const CallInst *AcquireCall) {
  bool FoundAcquire = false;
  for (Instruction &I : Block) {
    if (&I == AcquireCall) {
      FoundAcquire = true;
      continue;
    }
    if (FoundAcquire) {
      if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
        return GEP;
      }
    }
  }
  return nullptr;
}

static LoadInst *getLoadState(BasicBlock &Block, const CallInst *AcquireCall) {
  bool FoundAcquire = false;
  for (Instruction &I : Block) {
    if (&I == AcquireCall) {
      FoundAcquire = true;
      continue;
    }
    if (FoundAcquire) {
      if (LoadInst *Load = dyn_cast<LoadInst>(&I)) {
        if (Load->getPointerOperand()->getName().contains("State"))
          return Load;
      }
    }
  }
  return nullptr;
}

static ICmpInst *getComparison(BasicBlock &Block, const CallInst *AcquireCall) {
  bool FoundAcquire = false;
  for (Instruction &I : Block) {
    if (&I == AcquireCall) {
      FoundAcquire = true;
      continue;
    }
    if (FoundAcquire) {
      if (ICmpInst *Comparison = dyn_cast<ICmpInst>(&I)) {
        return Comparison;
      }
    }
  }
  return nullptr;
}

static LoadInst *getLoadP(BasicBlock &Block) {
  for (Instruction &I : Block) {
    if (LoadInst *Load = dyn_cast<LoadInst>(&I)) {
      if (Load->getPointerOperand()->getName().contains("P"))
        return Load;
    }
  }
  return nullptr;
}

static BasicBlock *getInnerLatch(DominatorTree &DT, Loop *L,
                                 BasicBlock *Header) {
  for (BasicBlock *Pred : predecessors(Header)) {
    if (!DT.dominates(Pred, Header))
      return Pred;
  }
  return nullptr;
}

PreservedAnalyses SchedPass::run(Function &F, FunctionAnalysisManager &AM) {
  if (F.getName() != "scheduler") {
    return PreservedAnalyses::all();
  }

  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

  // Iterate through the function's loops
  for (Loop *L : LI) {
    ArrayRef<BasicBlock *> BB = L->getBlocks();

    // Iterate through the blocks inside the loop
    for (const auto &B : BB) {
      // Find the acquire call
      CallInst *AcquireCall = getAcquireCall(*B);
      if (!AcquireCall)
        continue;

      // Find the inner loop's latch
      BasicBlock *InnerLatch = getInnerLatch(DT, L, B->getSinglePredecessor());
      if (!InnerLatch)
        continue;

      // Find the p->state GEP instruction after the acquire
      GetElementPtrInst *GEP = getGEP(*B, AcquireCall);
      if (!GEP)
        continue;

      // Find the Load for p->state after the acquire
      LoadInst *LoadState = getLoadState(*B, AcquireCall);
      if (!LoadState)
        continue;

      // Find a Load for p
      LoadInst *LoadP = getLoadP(*B);
      if (!LoadP)
        continue;

      // Find the comparison used after acquire
      ICmpInst *AcquireComparison = getComparison(*B, AcquireCall);
      if (!AcquireComparison)
        continue;

      // Apply the transformation
      IRBuilder<> Builder(B->getContext());

      // Split the original block to insert the bypass
      BasicBlock *ContinueBlock = B->splitBasicBlock(
          AcquireCall->getPrevNode()->getPrevNode(), "sched-pass.else");

      // Create the block inside the if with the continue in it
      BasicBlock *EarlyContinueBlock = BasicBlock::Create(
          B->getContext(), "sched-pass.then", &F, ContinueBlock);
      Builder.SetInsertPoint(EarlyContinueBlock);
      Builder.CreateBr(InnerLatch);

      // Load p->state to check in the bypass
      Builder.SetInsertPoint(B->getTerminator());

      LoadInst *P = Builder.CreateLoad(
          LoadP->getType(), LoadP->getPointerOperand(), "sched-pass.p-load");
      P->setAlignment(LoadP->getAlign());
      P->copyMetadata(*LoadP);

      Value *StatePtr = Builder.CreateGEP(
          GEP->getSourceElementType(), P,
          SmallVector<Value *, 4>(GEP->idx_begin(), GEP->idx_end()),
          "sched-pass.state-ptr");
      LoadInst *State = Builder.CreateLoad(LoadState->getType(), StatePtr,
                                           "sched-pass.state-load");
      State->setAlignment(LoadState->getAlign());
      State->copyMetadata(*LoadState);

      // Insert the if predicate and branch instruction
      Value *Condition = Builder.CreateICmpNE(
          State, AcquireComparison->getOperand(1), "sched-pass.cmp");
      Instruction *OldTerminator = B->getTerminator();
      Builder.CreateCondBr(Condition, EarlyContinueBlock, ContinueBlock);
      OldTerminator->eraseFromParent();

      return PreservedAnalyses::none();
    }
  }

  return PreservedAnalyses::all();
}
