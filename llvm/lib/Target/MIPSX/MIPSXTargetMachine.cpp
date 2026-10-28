//===-- MIPSXTargetMachine.cpp - Define TargetMachine for MIPS-X ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXTargetMachine.h"
#include "MIPSX.h"
#include "MIPSXMachineFunctionInfo.h"
#include "TargetInfo/MIPSXTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMIPSXTarget() {
  RegisterTargetMachine<MIPSXTargetMachine> X(getTheMIPSXTarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeMIPSXAsmPrinterPass(PR);
  initializeMIPSXDAGToDAGISelLegacyPass(PR);
  initializeMIPSXHazardPaddingPass(PR);
}

MIPSXTargetMachine::MIPSXTargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS, Options,
                               RM.value_or(Reloc::Static),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo();
}

MIPSXTargetMachine::~MIPSXTargetMachine() = default;

MachineFunctionInfo *MIPSXTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return MIPSXMachineFunctionInfo::create<MIPSXMachineFunctionInfo>(Allocator,
                                                                     F, STI);
}

namespace {
class MIPSXPassConfig : public TargetPassConfig {
public:
  MIPSXPassConfig(MIPSXTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  MIPSXTargetMachine &getMIPSXTargetMachine() const {
    return getTM<MIPSXTargetMachine>();
  }

  bool addInstSelector() override {
    addPass(createMIPSXISelDag(getMIPSXTargetMachine()));
    return false;
  }

  // Far branches (abi section 5.5); sizes include the padding added next.
  void addPreEmitPass() override { addPass(&BranchRelaxationPassID); }

  // Must be last: fills the delay slots and pads the pipeline hazards.
  void addPreEmitPass2() override { addPass(createMIPSXHazardPaddingPass()); }
};
} // namespace

TargetPassConfig *MIPSXTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new MIPSXPassConfig(*this, PM);
}
