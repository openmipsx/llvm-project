//===-- MIPSXFrameLowering.cpp - Frame lowering for MIPS-X ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Stack frame of MIPS-X SDK docs/abi.md section 8: the stack grows down, sp
// is 8-byte aligned, the outgoing argument area is at sp + 0 and reserved in
// the prologue, so sp does not move around calls. The prologue is a single
// "addi sp,#-size,sp" (through r1 for large frames); callee-saved registers,
// including r31 in functions that make calls, are saved by the generic
// spill code.
//
// TODO (work package S10): frame pointer, dynamic allocas, CFI.
//
//===----------------------------------------------------------------------===//

#include "MIPSXFrameLowering.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSXInstrInfo.h"
#include "MIPSXSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

bool MIPSXFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return MF.getFrameInfo().hasVarSizedObjects();
}

void MIPSXFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (MFI.hasVarSizedObjects())
    report_fatal_error("MIPS-X: dynamic stack allocation is not supported yet");

  uint64_t StackSize = alignTo(MFI.getStackSize(), getStackAlign());
  MFI.setStackSize(StackSize);
  if (StackSize == 0)
    return;

  MachineBasicBlock::iterator I = MBB.begin();
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  STI.getInstrInfo()->adjustReg(MBB, I, DL, MIPSX::R29, MIPSX::R29,
                                -int64_t(StackSize),
                                MachineInstr::FrameSetup);
}

void MIPSXFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  if (StackSize == 0)
    return;

  MachineBasicBlock::iterator I = MBB.getFirstTerminator();
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  STI.getInstrInfo()->adjustReg(MBB, I, DL, MIPSX::R29, MIPSX::R29,
                                int64_t(StackSize),
                                MachineInstr::FrameDestroy);
}

MachineBasicBlock::iterator MIPSXFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction & /*MF*/, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  // The call frame is part of the fixed frame (hasReservedCallFrame).
  return MBB.erase(I);
}

void MIPSXFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                              BitVector &SavedRegs,
                                              RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // A call overwrites r31, the return address.
  if (MF.getFrameInfo().hasCalls())
    SavedRegs.set(MIPSX::R31);
}
