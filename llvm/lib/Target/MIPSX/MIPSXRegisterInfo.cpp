//===-- MIPSXRegisterInfo.cpp - MIPS-X register information ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Register roles are those of MIPS-X SDK docs/abi.md section 2.
//
//===----------------------------------------------------------------------===//

#include "MIPSXRegisterInfo.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSXFrameLowering.h"
#include "MIPSXInstrInfo.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

#define GET_REGINFO_TARGET_DESC
#include "MIPSXGenRegisterInfo.inc"

using namespace llvm;

MIPSXRegisterInfo::MIPSXRegisterInfo() : MIPSXGenRegisterInfo(MIPSX::R31) {}

const MCPhysReg *
MIPSXRegisterInfo::getCalleeSavedRegs(const MachineFunction * /*MF*/) const {
  return CSR_MIPSX_SaveList;
}

const uint32_t *
MIPSXRegisterInfo::getCallPreservedMask(const MachineFunction & /*MF*/,
                                        CallingConv::ID /*CC*/) const {
  return CSR_MIPSX_RegMask;
}

BitVector MIPSXRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // r0 is zero, r1 the assembler temporary, r23/r24 the code bases, r28 the
  // global pointer and r29 the stack pointer.
  for (MCPhysReg Reg : {MIPSX::R0, MIPSX::R1, MIPSX::R23, MIPSX::R24,
                        MIPSX::R28, MIPSX::R29, MIPSX::PSW, MIPSX::MD,
                        MIPSX::PCM4, MIPSX::PCM1})
    Reserved.set(Reg);
  if (getFrameLowering(MF)->hasFP(MF))
    Reserved.set(MIPSX::R30);
  return Reserved;
}

Register
MIPSXRegisterInfo::getFrameRegister(const MachineFunction & /*MF*/) const {
  return MIPSX::R29;
}

// Every instruction that takes a frame index (loads, stores and addi) has it
// as a base register followed by a 17-bit offset.
bool MIPSXRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger * /*RS*/) const {
  assert(SPAdj == 0 && "Unexpected SP adjustment");
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  int FI = MI.getOperand(FIOperandNum).getIndex();
  int64_t Offset = MFI.getObjectOffset(FI) + MFI.getStackSize() +
                   MI.getOperand(FIOperandNum + 1).getImm();
  Register Base = MIPSX::R29;
  if (!isInt<17>(Offset)) {
    // Compute sp + Offset in r1, which is reserved for this (abi section
    // 8.4).
    const auto &TII = *static_cast<const MIPSXInstrInfo *>(
        MF.getSubtarget().getInstrInfo());
    TII.adjustReg(*MI.getParent(), II, MI.getDebugLoc(), MIPSX::R1,
                  MIPSX::R29, Offset, MachineInstr::NoFlags);
    Base = MIPSX::R1;
    Offset = 0;
  }

  MI.getOperand(FIOperandNum).ChangeToRegister(Base, false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
  return false;
}
