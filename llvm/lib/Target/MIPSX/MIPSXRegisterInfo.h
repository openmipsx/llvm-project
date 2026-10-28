//===-- MIPSXRegisterInfo.h - MIPS-X register information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXREGISTERINFO_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "MIPSXGenRegisterInfo.inc"

namespace llvm {

struct MIPSXRegisterInfo : public MIPSXGenRegisterInfo {
  MIPSXRegisterInfo();

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID) const override;
  BitVector getReservedRegs(const MachineFunction &MF) const override;

  /// The registers preserved by an SDK helper routine (__mulsi3 and so on),
  /// or null if Name is not one with a known clobber list.
  const uint32_t *getHelperCallPreservedMask(StringRef Name) const;

  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXREGISTERINFO_H
