//===-- MIPSXFrameLowering.cpp - Frame lowering for MIPS-X ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Stack frame of MIPS-X SDK docs/abi.md section 8: the stack grows down, sp
// is 8-byte aligned, and the outgoing argument area is at sp + 0. Without
// dynamic allocas the call frame is reserved in the prologue, so sp does not
// move around calls; with them, each call adjusts sp around itself.
//
// The prologue is "addi sp,#-size,sp" (through r1 for large frames) and the
// callee-saved stores. A function with a frame pointer then sets fp = sp +
// size, the CFA, and addresses its frame from fp; r31 and the caller's r30
// are at fp - 4 and fp - 8, so frames form a chain. Its epilogue resets sp
// from fp before restoring registers.
//
// Objects aligned beyond 8 bytes realign the stack: the prologue then rounds
// sp down to that alignment after setting fp. The body addresses fixed
// objects (incoming arguments, r31 and r30) from fp and the others from the
// aligned sp; the callee-saved stores and loads use the unaligned sp in the
// prologue and epilogue. The callee-saved slots lie above every other
// object, so moving the others down does not make them overlap.
//
// TODO (work package S17): CFI.
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
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

bool MIPSXFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken() ||
         MF.getTarget().Options.DisableFramePointerElim(MF) ||
         STI.getRegisterInfo()->hasStackRealignment(MF);
}

void MIPSXFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  bool Realign = STI.getRegisterInfo()->hasStackRealignment(MF);
  if (MFI.getMaxAlign() > getStackAlign() && !Realign)
    report_fatal_error("MIPS-X: stack objects aligned beyond 8 bytes need "
                       "stack realignment");
  // Dynamic allocas would need a base pointer as well.
  if (Realign && MFI.hasVarSizedObjects())
    report_fatal_error("MIPS-X: stack realignment with dynamic allocas is not "
                       "supported");
  if (Realign && MFI.getMaxAlign() > Align(1 << 16))
    report_fatal_error("MIPS-X: stack objects aligned beyond 64 KB are not "
                       "supported");

  uint64_t StackSize = alignTo(MFI.getStackSize(), getStackAlign());
  MFI.setStackSize(StackSize);
  if (StackSize == 0)
    return;

  const MIPSXInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator I = MBB.begin();
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  TII.adjustReg(MBB, I, DL, MIPSX::R29, MIPSX::R29, -int64_t(StackSize),
                MachineInstr::FrameSetup);

  if (!hasFP(MF))
    return;
  // fp = CFA, after the callee-saved stores (which save the caller's fp).
  while (I != MBB.end() && I->getFlag(MachineInstr::FrameSetup))
    ++I;
  TII.adjustReg(MBB, I, DL, MIPSX::R30, MIPSX::R29, int64_t(StackSize),
                MachineInstr::FrameSetup);
  if (!Realign)
    return;
  // sp &= -MaxAlign, the mask built in r1.
  BuildMI(MBB, I, DL, TII.get(MIPSX::ADDI), MIPSX::R1)
      .addReg(MIPSX::R0)
      .addImm(-int64_t(MFI.getMaxAlign().value()))
      .setMIFlag(MachineInstr::FrameSetup);
  BuildMI(MBB, I, DL, TII.get(MIPSX::AND), MIPSX::R29)
      .addReg(MIPSX::R29)
      .addReg(MIPSX::R1)
      .setMIFlag(MachineInstr::FrameSetup);
}

void MIPSXFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  if (StackSize == 0)
    return;

  const MIPSXInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator I = MBB.getFirstTerminator();
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();

  if (hasFP(MF)) {
    // sp may have moved (dynamic allocas): reset it from fp before the
    // callee-saved restores, which address their slots from sp.
    MachineBasicBlock::iterator R = I;
    while (R != MBB.begin() &&
           std::prev(R)->getFlag(MachineInstr::FrameDestroy))
      --R;
    TII.adjustReg(MBB, R, DL, MIPSX::R29, MIPSX::R30, -int64_t(StackSize),
                  MachineInstr::FrameDestroy);
  }

  TII.adjustReg(MBB, I, DL, MIPSX::R29, MIPSX::R29, int64_t(StackSize),
                MachineInstr::FrameDestroy);
}

MachineBasicBlock::iterator MIPSXFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  // With a reserved call frame the outgoing area is part of the fixed frame;
  // otherwise (dynamic allocas) sp moves around the call.
  if (!hasReservedCallFrame(MF)) {
    int64_t Amount = alignTo(I->getOperand(0).getImm(), getStackAlign());
    if (Amount != 0) {
      if (I->getOpcode() == MIPSX::ADJCALLSTACKDOWN)
        Amount = -Amount;
      STI.getInstrInfo()->adjustReg(MBB, I, I->getDebugLoc(), MIPSX::R29,
                                    MIPSX::R29, Amount,
                                    MachineInstr::NoFlags);
    }
  }
  return MBB.erase(I);
}

void MIPSXFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                              BitVector &SavedRegs,
                                              RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // A call overwrites r31, the return address. A frame pointer saves the
  // caller's, and the return address beside it.
  if (MF.getFrameInfo().hasCalls())
    SavedRegs.set(MIPSX::R31);
  if (hasFP(MF)) {
    SavedRegs.set(MIPSX::R30);
    SavedRegs.set(MIPSX::R31);
  }
}

const TargetFrameLowering::SpillSlot *
MIPSXFrameLowering::getCalleeSavedSpillSlots(unsigned &NumEntries) const {
  static const SpillSlot Offsets[] = {{MIPSX::R31, -4}, {MIPSX::R30, -8}};
  NumEntries = std::size(Offsets);
  return Offsets;
}

bool MIPSXFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const MIPSXInstrInfo &TII = *STI.getInstrInfo();
  const MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  for (const CalleeSavedInfo &CS : CSI) {
    MCRegister Reg = CS.getReg();
    // r31 is also a function live-in when __builtin_return_address reads
    // it; then the spill must not kill it.
    bool IsLiveIn = MRI.isLiveIn(Reg);
    if (!IsLiveIn)
      MBB.addLiveIn(Reg);
    TII.storeRegToStackSlot(MBB, MI, Reg, /*IsKill=*/!IsLiveIn, CS.getFrameIdx(),
                            &MIPSX::GPRRegClass, Register(),
                            MachineInstr::FrameSetup);
  }
  return true;
}

bool MIPSXFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const MIPSXInstrInfo &TII = *STI.getInstrInfo();
  for (const CalleeSavedInfo &CS : reverse(CSI))
    TII.loadRegFromStackSlot(MBB, MI, CS.getReg(), CS.getFrameIdx(),
                             &MIPSX::GPRRegClass, Register(), 0,
                             MachineInstr::FrameDestroy);
  return true;
}
