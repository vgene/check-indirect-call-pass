#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/Statistic.h"

#include <set>

#define DEBUG_TYPE "check-icall"

using namespace llvm;
STATISTIC(NumICall, "Number of indirect call in the program");
STATISTIC(NumCoveredByDevirt, "Number of indirect call covered by CPF devirt");
STATISTIC(NumCoveredByPgo, "Number of indirect call covered by pgo-icall-prom");


namespace {

  struct CAT : public FunctionPass {
    static char ID; 
    
    // static StringRef getFunctionName(CallBase *call)
    // {
    //     Function *fun = call->getCalledFunction();
    //     if (fun) // thanks @Anton Korobeynikov
    //         return fun->getName(); // inherited from llvm::Value
    //     else
    //         return StringRef("indirect call");
    // }

    CAT() : FunctionPass(ID) {}

    // This function is invoked once at the initialization phase of the compiler
    // The LLVM IR of functions isn't ready at this point
    bool doInitialization (Module &M) override {
      return false;
    }

    // This function is invoked once per function compiled
    // The LLVM IR of the input functions is ready and it can be analyzed and/or transformed
    bool runOnFunction (Function &F) override {
      // errs() << "Hello LLVM World at \"runOnFunction\"\n" ;

      for (BasicBlock &bb: F){
        for (Instruction &I: bb){
          // errs() << "Visiting" << bb << "\n";
          if (CallBase* cs = dyn_cast<CallBase>(&I)){
            Function *fn = cs->getCalledFunction();
            if (!fn) {// indirect call
              NumICall++;
              if (!bb.hasName()) // doesn't have a name
                continue;
              // if the basic block starts with "default_indirect", then it's covered by CPF's devirt
              if (bb.getName().startswith("default_indirect")){
                NumCoveredByDevirt++;
                continue;
              }

              // if the basic block starts with "if.false" then it's covered by LLVM pgo-icall-prom
              if (bb.getName().startswith("if.false")){
                NumCoveredByPgo++;
                continue;
              }

              // else it's a not covered case
              errs() << I << "\n";
            }

          }
        }
      }

      return true;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      // AU.setPreservesAll();
    }
  };
}

// Next there is code to register your pass to "opt"
char CAT::ID = 0;
static RegisterPass<CAT> X("check-icall", "Check all indirect call",
    false, // cfg only
    true // is analysis
    );

// Next there is code to register your pass to "clang"
static CAT * _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT()); }}); // ** for -O0
