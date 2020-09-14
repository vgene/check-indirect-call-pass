#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ProfileData/InstrProf.h"

#include <set>

#define DEBUG_TYPE "check-icall"
#define MaxNumPromotions 100

using namespace llvm;
STATISTIC(NumICall, "Number of indirect call in the program");
STATISTIC(NumCoveredByDevirt, "Number of indirect call covered by CPF devirt");
STATISTIC(NumCoveredByPgo, "Number of indirect call covered by pgo-icall-prom");
STATISTIC(NumUnexercised, "Number of indirect call not covered but not exercised by profile input");

static unsigned long numICall = 0;
static unsigned long numCoveredByDevirt = 0;
static unsigned long numCoveredByPgo = 0;
static unsigned long numUnexercised = 0;


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
      std::unique_ptr<InstrProfValueData[]> ValueDataArray = llvm::make_unique<InstrProfValueData[]>(MaxNumPromotions);
      uint32_t NumVals;
      uint64_t TotalCount;

      for (BasicBlock &bb: F){
        for (Instruction &I: bb){
          // errs() << "Visiting" << bb << "\n";
          if (CallBase* cs = dyn_cast<CallBase>(&I)){
            Function *fn = cs->getCalledFunction();
            if (!fn) {// indirect call
              NumICall++;
              numICall++;
              if (!bb.hasName()) // doesn't have a name
                continue;
              // if the basic block starts with "default_indirect", then it's covered by CPF's devirt
              if (bb.getName().startswith("default_indirect")){
                NumCoveredByDevirt++;
                numCoveredByDevirt++;
                continue;
              }

              // if the basic block starts with "if.false" then it's covered by LLVM pgo-icall-prom
              if (bb.getName().startswith("if.false")){
                NumCoveredByPgo++;
                numCoveredByPgo++;
                continue;
              }

              // get value pred
              bool Res =
                getValueProfDataFromInst(I, IPVK_IndirectCallTarget, MaxNumPromotions,
                    ValueDataArray.get(), NumVals, TotalCount);
              if (!Res) {
                NumUnexercised++;
                numUnexercised++;
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

    bool doFinalization(Module &M) override {
      errs() << "Num Indirect Call:" << numICall << "\n";
      errs() << "Num Indirect Call covered by devirt:" << numCoveredByDevirt << "\n";
      errs() << "Num Indirect Call covered by PGO:" << numCoveredByPgo << "\n";
      errs() << "Num Indirect Call not covered but also not exercised:" << numUnexercised<< "\n";

      return false;
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
