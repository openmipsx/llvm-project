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
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "MIPSXGenInstrInfo.inc"

// For testing branch relaxation without 128 KB functions.
static cl::opt<unsigned> BranchDispBits(
    "mipsx-branch-disp-bits", cl::Hidden, cl::init(16),
    cl::desc("Bits of branch displacement assumed by branch relaxation"));

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
  BuildMI(MBB, I, DL, get(MIPSX::LDfi), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(
          getFrameMemOperand(MBB, FrameIndex, MachineMemOperand::MOLoad))
      .setMIFlag(Flags);
}

Register MIPSXInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  if ((MI.getOpcode() == MIPSX::LD || MI.getOpcode() == MIPSX::LDfi) &&
      MI.getOperand(1).isFI() &&
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

static bool isCondBranch(unsigned Opc) {
  switch (Opc) {
  case MIPSX::BEQ:
  case MIPSX::BNE:
  case MIPSX::BLT:
  case MIPSX::BGE:
  case MIPSX::BLO:
  case MIPSX::BHS:
    return true;
  default:
    return false;
  }
}

static void parseCondBranch(MachineInstr &MI, MachineBasicBlock *&Target,
                            SmallVectorImpl<MachineOperand> &Cond) {
  Target = MI.getOperand(2).getMBB();
  Cond.push_back(MachineOperand::CreateImm(MI.getOpcode()));
  Cond.push_back(MI.getOperand(0));
  Cond.push_back(MI.getOperand(1));
}

bool MIPSXInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Count the terminators and find the first.
  MachineBasicBlock::iterator FirstUncond = MBB.end();
  int NumTerminators = 0;
  for (auto J = I.getReverse(); J != MBB.rend() && isUnpredicatedTerminator(*J);
       ++J) {
    ++NumTerminators;
    if (J->getDesc().isUnconditionalBranch() ||
        J->getDesc().isIndirectBranch())
      FirstUncond = J.getReverse();
  }

  // Delete anything after the first unconditional branch.
  if (AllowModify && FirstUncond != MBB.end()) {
    while (std::next(FirstUncond) != MBB.end()) {
      std::next(FirstUncond)->eraseFromParent();
      --NumTerminators;
    }
    I = FirstUncond;
  }

  // Indirect branches, returns and far jumps are not analysable.
  if (I->getDesc().isIndirectBranch() || I->isReturn() ||
      I->getOpcode() == MIPSX::JMP)
    return true;

  if (NumTerminators > 2)
    return true;

  if (NumTerminators == 1 && I->getOpcode() == MIPSX::B) {
    TBB = I->getOperand(0).getMBB();
    return false;
  }
  if (NumTerminators == 1 && isCondBranch(I->getOpcode())) {
    parseCondBranch(*I, TBB, Cond);
    return false;
  }
  if (NumTerminators == 2 && isCondBranch(std::prev(I)->getOpcode()) &&
      I->getOpcode() == MIPSX::B) {
    parseCondBranch(*std::prev(I), TBB, Cond);
    FBB = I->getOperand(0).getMBB();
    return false;
  }
  return true;
}

unsigned MIPSXInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  unsigned Count = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  while (I != MBB.end() &&
         (I->getOpcode() == MIPSX::B || isCondBranch(I->getOpcode()))) {
    if (BytesRemoved)
      *BytesRemoved += getInstSizeInBytes(*I);
    I->eraseFromParent();
    ++Count;
    I = MBB.getLastNonDebugInstr();
  }
  return Count;
}

unsigned MIPSXInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                      MachineBasicBlock *TBB,
                                      MachineBasicBlock *FBB,
                                      ArrayRef<MachineOperand> Cond,
                                      const DebugLoc &DL,
                                      int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  unsigned Count = 0;
  int Bytes = 0;
  if (Cond.empty()) {
    MachineInstr &MI = *BuildMI(&MBB, DL, get(MIPSX::B)).addMBB(TBB);
    Bytes += getInstSizeInBytes(MI);
    ++Count;
  } else {
    MachineInstr &MI = *BuildMI(&MBB, DL, get(Cond[0].getImm()))
                            .add(Cond[1])
                            .add(Cond[2])
                            .addMBB(TBB);
    Bytes += getInstSizeInBytes(MI);
    ++Count;
    if (FBB) {
      MachineInstr &MI2 = *BuildMI(&MBB, DL, get(MIPSX::B)).addMBB(FBB);
      Bytes += getInstSizeInBytes(MI2);
      ++Count;
    }
  }
  if (BytesAdded)
    *BytesAdded = Bytes;
  return Count;
}

bool MIPSXInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  unsigned Opc;
  switch (Cond[0].getImm()) {
  case MIPSX::BEQ:
    Opc = MIPSX::BNE;
    break;
  case MIPSX::BNE:
    Opc = MIPSX::BEQ;
    break;
  case MIPSX::BLT:
    Opc = MIPSX::BGE;
    break;
  case MIPSX::BGE:
    Opc = MIPSX::BLT;
    break;
  case MIPSX::BLO:
    Opc = MIPSX::BHS;
    break;
  case MIPSX::BHS:
    Opc = MIPSX::BLO;
    break;
  default:
    return true;
  }
  Cond[0].setImm(Opc);
  return false;
}

// Sizes include the nops MIPSXHazardPadding adds later: two after anything
// with delay slots, one after a load.
unsigned MIPSXInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isMetaInstruction())
    return 0;
  if (MI.isInlineAsm()) {
    const MachineFunction &MF = *MI.getParent()->getParent();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              MF.getTarget().getMCAsmInfo());
  }
  if (MI.getOpcode() == MIPSX::MULSTEPS) // And a nop that may come before.
    return 4 * (MI.getOperand(3).getImm() + 3);
  if (MI.hasDelaySlot())
    return 12;
  if (MI.mayLoad())
    return 8;
  return 4;
}

bool MIPSXInstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
                                           int64_t BrOffset) const {
  if (BranchOpc == MIPSX::JMP)
    return true; // Through a code base (abi section 5.2).
  // Target word = branch word + 2 + displacement.
  int64_t Disp = BrOffset / 4 - 2;
  return isIntN(BranchDispBits, Disp);
}

MachineBasicBlock *
MIPSXInstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case MIPSX::B:
  case MIPSX::JMP:
    return MI.getOperand(0).getMBB();
  default:
    assert(isCondBranch(MI.getOpcode()) && "Not a branch");
    return MI.getOperand(2).getMBB();
  }
}

// A branch that cannot reach becomes jspci r0,#%call(label),r0, which needs
// no scratch register (abi section 5.5).
void MIPSXInstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                          MachineBasicBlock &NewDestBB,
                                          MachineBasicBlock & /*RestoreBB*/,
                                          const DebugLoc &DL,
                                          int64_t /*BrOffset*/,
                                          RegScavenger * /*RS*/) const {
  BuildMI(&MBB, DL, get(MIPSX::JMP)).addMBB(&NewDestBB, MIPSXII::MO_CALL);
}
