//===-- MIPSXInstrInfo.cpp - MIPS-X instruction information ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXInstrInfo.h"
#include "MIPSXSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "MIPSXGenInstrInfo.inc"

MIPSXInstrInfo::MIPSXInstrInfo(const MIPSXSubtarget &STI)
    : MIPSXGenInstrInfo(STI, RI, MIPSX::ADJCALLSTACKDOWN,
                        MIPSX::ADJCALLSTACKUP),
      RI() {}

void MIPSXInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator I,
                                 const DebugLoc &DL, Register DestReg,
                                 Register SrcReg, bool KillSrc,
                                 bool /*RenamableDest*/,
                                 bool /*RenamableSrc*/) const {
  if (!MIPSX::GPRRegClass.contains(DestReg, SrcReg))
    llvm_unreachable("Impossible register-to-register copy");
  // mov rs,rd is add rs,r0,rd.
  BuildMI(MBB, I, DL, get(MIPSX::ADD), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addReg(MIPSX::R0);
}

static MachineMemOperand *getFrameMemOperand(MachineBasicBlock &MBB, int FI,
                                             MachineMemOperand::Flags Flags) {
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getMachineMemOperand(MachinePointerInfo::getFixedStack(MF, FI),
                                 Flags, MFI.getObjectSize(FI),
                                 MFI.getObjectAlign(FI));
}

void MIPSXInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC,
    Register /*VReg*/, MachineInstr::MIFlag Flags) const {
  if (!MIPSX::GPRRegClass.hasSubClassEq(RC))
    llvm_unreachable("Can't store this register to a stack slot");
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  BuildMI(MBB, I, DL, get(MIPSX::ST))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(
          getFrameMemOperand(MBB, FrameIndex, MachineMemOperand::MOStore))
      .setMIFlag(Flags);
}

void MIPSXInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register /*VReg*/,
    unsigned /*SubReg*/, MachineInstr::MIFlag Flags) const {
  if (!MIPSX::GPRRegClass.hasSubClassEq(RC))
    llvm_unreachable("Can't load this register from a stack slot");
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  BuildMI(MBB, I, DL, get(MIPSX::LD), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(
          getFrameMemOperand(MBB, FrameIndex, MachineMemOperand::MOLoad))
      .setMIFlag(Flags);
}

Register MIPSXInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  if (MI.getOpcode() == MIPSX::LD && MI.getOperand(1).isFI() &&
      MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }
  return Register();
}

Register MIPSXInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  if (MI.getOpcode() == MIPSX::ST && MI.getOperand(1).isFI() &&
      MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }
  return Register();
}

void MIPSXInstrInfo::adjustReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I,
                               const DebugLoc &DL, Register Dest,
                               Register Base, int64_t Imm,
                               MachineInstr::MIFlag Flag) const {
  if (isInt<17>(Imm)) {
    BuildMI(MBB, I, DL, get(MIPSX::ADDI), Dest)
        .addReg(Base)
        .addImm(Imm)
        .setMIFlag(Flag);
    return;
  }
  // Build Imm in r1 with the hi/lo form (docs/abi.md section 6.1), then add.
  int32_t V = static_cast<int32_t>(Imm);
  BuildMI(MBB, I, DL, get(MIPSX::ADDI), MIPSX::R1)
      .addReg(MIPSX::R0)
      .addImm(V >> 15)
      .setMIFlag(Flag);
  BuildMI(MBB, I, DL, get(MIPSX::SH), MIPSX::R1)
      .addReg(MIPSX::R0)
      .addReg(MIPSX::R1)
      .addImm(32 - 15)
      .setMIFlag(Flag);
  BuildMI(MBB, I, DL, get(MIPSX::ADDI), MIPSX::R1)
      .addReg(MIPSX::R1)
      .addImm(V & 0x7fff)
      .setMIFlag(Flag);
  BuildMI(MBB, I, DL, get(MIPSX::ADD), Dest)
      .addReg(Base)
      .addReg(MIPSX::R1)
      .setMIFlag(Flag);
}
