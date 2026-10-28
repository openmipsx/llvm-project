//===-- MIPSXFrameLowering.h - Frame lowering for MIPS-X --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXFRAMELOWERING_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class MIPSXSubtarget;

class MIPSXFrameLowering : public TargetFrameLowering {
  const MIPSXSubtarget &STI;

public:
  explicit MIPSXFrameLowering(const MIPSXSubtarget &STI)
      : TargetFrameLowering(StackGrowsDown, /*StackAlignment=*/Align(8),
                            /*LocalAreaOffset=*/0),
        STI(STI) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS = nullptr) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXFRAMELOWERING_H
