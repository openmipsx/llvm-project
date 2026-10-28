//===-- MIPSXInstrInfo.h - MIPS-X instruction information -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXINSTRINFO_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXINSTRINFO_H

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSXRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "MIPSXGenInstrInfo.inc"

namespace llvm {

class MIPSXSubtarget;

namespace MIPSXII {
// Target flags on symbol operands.
enum TOF {
  MO_NO_FLAG,
  MO_HI, // %hi(sym)
  MO_LO, // %lo(sym)
  MO_CALL, // %call(sym)
  MO_GPREL, // %gprel(sym)
};
} // namespace MIPSXII

class MIPSXInstrInfo : public MIPSXGenInstrInfo {
  const MIPSXRegisterInfo RI;

public:
  explicit MIPSXInstrInfo(const MIPSXSubtarget &STI);

  const MIPSXRegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register SrcReg,
      bool IsKill, int FrameIndex, const TargetRegisterClass *RC,
      Register VReg,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register DestReg,
      int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      unsigned SubReg = 0,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  Register isLoadFromStackSlot(const MachineInstr &MI,
                               int &FrameIndex) const override;
  Register isStoreToStackSlot(const MachineInstr &MI,
                              int &FrameIndex) const override;

  // Branches. Cond is {branch opcode, rs1, rs2}.
  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;
  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;
  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;
  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  // Branch relaxation (abi section 5.5).
  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;
  bool isBranchOffsetInRange(unsigned BranchOpc,
                             int64_t BrOffset) const override;
  MachineBasicBlock *getBranchDestBlock(const MachineInstr &MI) const override;
  void insertIndirectBranch(MachineBasicBlock &MBB,
                            MachineBasicBlock &NewDestBB,
                            MachineBasicBlock &RestoreBB, const DebugLoc &DL,
                            int64_t BrOffset = 0,
                            RegScavenger *RS = nullptr) const override;

  /// Add "addi Base,#Imm,Dest" before I, using r1 if Imm needs more than 17
  /// bits.
  void adjustReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                 const DebugLoc &DL, Register Dest, Register Base,
                 int64_t Imm, MachineInstr::MIFlag Flag) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXINSTRINFO_H
