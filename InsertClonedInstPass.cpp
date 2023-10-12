#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace {
  struct InsertClonedInstPass : public FunctionPass {
    static char ID;
    InsertClonedInstPass() : FunctionPass(ID) {}

    bool isExcludedInstruction(Instruction *inst) {
      if (isa<AllocaInst>(inst))
        return true;
      if (CallInst *call = dyn_cast<CallInst>(inst)) {
        if (!call->getType()->isVoidTy())
          return true;
      }
      if (isa<LoadInst>(inst) || isa<StoreInst>(inst) || isa<AtomicCmpXchgInst>(inst))
        return true;
      return false;
    }

    bool runOnFunction(Function &F) override {
        LLVMContext &Ctx = F.getContext();
        errs() << "Running on function: " << F.getName() << "\n";
        
        bool modified = false;
        unsigned blockCounter = 0;

        FunctionCallee printfFunc = F.getParent()->getOrInsertFunction("printf", FunctionType::get(IntegerType::getInt32Ty(Ctx), PointerType::get(Type::getInt8Ty(Ctx), 0), true /* this is var arg func type*/ ));

        for (auto &BB : F) {
            if (BranchInst* branch = dyn_cast<BranchInst>(BB.getTerminator())) {
                Instruction* prevInst = branch->getPrevNode();
                while (prevInst && (prevInst->getType()->isVoidTy() || isExcludedInstruction(prevInst))) {
                    prevInst = prevInst->getPrevNode();
                }

                if (!prevInst) {
                    continue;
                }

                Instruction *newInst = prevInst->clone();
                newInst->insertAfter(prevInst);

                IRBuilder<> builder(newInst->getNextNode());
                Value *cmp;
                std::string baseName = std::to_string(blockCounter);

                if (prevInst->getType()->isIntegerTy() || prevInst->getType()->isPointerTy()) {
                    cmp = builder.CreateICmpEQ(prevInst, newInst, "cmp_result" + baseName);
                } else if (prevInst->getType()->isFloatingPointTy()) {
                    cmp = builder.CreateFCmpOEQ(prevInst, newInst, "cmp_result" + baseName);
                } else {
                    continue; 
                }

                std::string thenName = "then" + baseName;
                std::string elseName = "else" + baseName;
                BasicBlock* thenBB = BB.splitBasicBlock(builder.GetInsertPoint(), thenName);
                BasicBlock* elseBB = BasicBlock::Create(Ctx, elseName, &F, thenBB);
                BB.getTerminator()->eraseFromParent();

                builder.SetInsertPoint(&BB);
                builder.CreateCondBr(cmp, thenBB, elseBB);

                // Error reporting in the else block
                IRBuilder<> elseBuilder(elseBB);
                Value* funcName = elseBuilder.CreateGlobalStringPtr(F.getName());
                Value* blockIdValue = ConstantInt::get(Type::getInt32Ty(Ctx), blockCounter);
                Value* instName = elseBuilder.CreateGlobalStringPtr(prevInst->getOpcodeName());
                Value* formatStr;

                if (prevInst->getType()->isIntegerTy(32)) {
                    formatStr = elseBuilder.CreateGlobalStringPtr("Error in function %s, block %d, instruction: %s. Original value: %d, Duplicated value: %d\n");
                } else if (prevInst->getType()->isIntegerTy(64)) {
                    formatStr = elseBuilder.CreateGlobalStringPtr("Error in function %s, block %d, instruction: %s. Original value: %lld, Duplicated value: %lld\n");
                } else if (prevInst->getType()->isDoubleTy()) {
                    formatStr = elseBuilder.CreateGlobalStringPtr("Error in function %s, block %d, instruction: %s. Original value: %f, Duplicated value: %f\n");
                } else {
                    formatStr = elseBuilder.CreateGlobalStringPtr("Error in function %s, block %d, instruction: %s. Mismatch detected.\n");
                }

                if (prevInst->getType()->isIntegerTy(32) || prevInst->getType()->isIntegerTy(64) || prevInst->getType()->isDoubleTy()) {
                    elseBuilder.CreateCall(printfFunc, { formatStr, funcName, blockIdValue, instName, prevInst, newInst });
                } else {
                    elseBuilder.CreateCall(printfFunc, { formatStr, funcName, blockIdValue, instName });
                }

                elseBuilder.CreateUnreachable();
                modified = true;
                blockCounter++;
            }
        }

        return modified;
    }
  };
}

char InsertClonedInstPass::ID = 0;
static RegisterPass<InsertClonedInstPass> X("insert-cloned-inst", "Insert cloned instructions with check");

