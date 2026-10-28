//===-- MIPSXMachineFunctionInfo.h - MIPS-X machine function info -*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MIPSXMachineFunctionInfo : public MachineFunctionInfo {
  // The fixed stack object at the first variadic argument word (va_start).
  int VarArgsFrameIndex = 0;

public:
  MIPSXMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {
  }

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<MIPSXMachineFunctionInfo>(*this);
  }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int FI) { VarArgsFrameIndex = FI; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXMACHINEFUNCTIONINFO_H
