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

  // The base register and offset of a frame object in the function body.
  StackOffset getFrameIndexReference(const MachineFunction &MF, int FI,
                                     Register &FrameReg) const override;

  // The outgoing argument area is part of the fixed frame unless dynamic
  // allocas move sp; a frame pointer alone does not. A realigned frame
  // addresses its locals from sp, so sp must not move around calls there.
  bool hasReservedCallFrame(const MachineFunction &MF) const override {
    return !MF.getFrameInfo().hasVarSizedObjects();
  }

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS = nullptr) const override;

  // r31 and r30 are saved at CFA - 4 and CFA - 8 (abi section 8.2).
  const SpillSlot *
  getCalleeSavedSpillSlots(unsigned &NumEntries) const override;

  // The saves and restores are marked FrameSetup and FrameDestroy, so that
  // their frame indices are resolved against sp, which is valid there.
  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;
  bool
  restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              MutableArrayRef<CalleeSavedInfo> CSI,
                              const TargetRegisterInfo *TRI) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXFRAMELOWERING_H
