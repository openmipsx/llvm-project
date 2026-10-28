//===-- MIPSXTargetMachine.h - Define TargetMachine for MIPS-X --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXTARGETMACHINE_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXTARGETMACHINE_H

#include "MIPSXSubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include <optional>

namespace llvm {

class MIPSXTargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  MIPSXSubtarget Subtarget;

public:
  MIPSXTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                     StringRef FS, const TargetOptions &Options,
                     std::optional<Reloc::Model> RM,
                     std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                     bool JIT);
  ~MIPSXTargetMachine() override;

  const MIPSXSubtarget *getSubtargetImpl(const Function &) const override {
    return &Subtarget;
  }
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXTARGETMACHINE_H
