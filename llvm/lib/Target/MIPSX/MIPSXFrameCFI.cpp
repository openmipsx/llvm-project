//===-- MIPSXFrameCFI.cpp - Call frame information for the prologue -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Adds the prologue's CFI (MIPS-X SDK docs/abi.md section 8.6) when the
// function needs frame moves (-g or unwind tables). It runs after the
// post-RA scheduler, because CFI instructions are scheduling boundaries and
// would otherwise make -g change the code. The prologue is then the
// FrameSetup instructions of the entry block, possibly interleaved with the
// body, and each rule follows the instruction it describes:
//
// - the sp adjustment (its last instruction for a large frame): the CFA is
//   sp + size;
// - each callee-saved store: the register is at its slot; r31 holds a word
//   address, so its rule multiplies by 4 (MIPSX::createReturnAddressCFI);
// - fp = sp + size, with a frame pointer: the CFA is fp + 0.
//
// Epilogues get no CFI: the delay-slot filler moves their sp adjustment into
// the return's slots, and a block placed after an epilogue still needs the
// prologue's rules. The filler looks through CFI, so it may move a prologue
// instruction past its rule into a later branch's slots; the rule then holds
// from the branch, up to two instructions early, and is exact at every
// call. In exchange, CFI never changes the code.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSX.h"
#include "MIPSXInstrInfo.h"
#include "MIPSXSubtarget.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDwarf.h"

using namespace llvm;

#define DEBUG_TYPE "mipsx-frame-cfi"

namespace {

class MIPSXFrameCFI : public MachineFunctionPass {
public:
  static char ID;
  MIPSXFrameCFI() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "MIPS-X frame CFI"; }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // end anonymous namespace

char MIPSXFrameCFI::ID = 0;

INITIALIZE_PASS(MIPSXFrameCFI, DEBUG_TYPE, "MIPS-X frame CFI", false, false)

static void insertCFIAfter(MachineInstr &MI, const MCCFIInstruction &Inst) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  BuildMI(MBB, std::next(MI.getIterator()), MI.getDebugLoc(),
          TII.get(TargetOpcode::CFI_INSTRUCTION))
      .addCFIIndex(MF.addFrameInst(Inst))
      .setMIFlag(MachineInstr::FrameSetup);
}

bool MIPSXFrameCFI::runOnMachineFunction(MachineFunction &MF) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  if (!MF.needsFrameMoves() || MFI.getStackSize() == 0)
    return false;

  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  bool HasFP = MF.getSubtarget().getFrameLowering()->hasFP(MF);
  bool SeenSP = false, SeenFP = false;
  SmallSet<Register, 16> Saved;

  for (MachineInstr &MI : make_early_inc_range(MF.front())) {
    if (!MI.getFlag(MachineInstr::FrameSetup) || MI.isCFIInstruction())
      continue;
    if (!SeenSP && MI.modifiesRegister(MIPSX::R29, TRI)) {
      SeenSP = true;
      insertCFIAfter(MI, MCCFIInstruction::cfiDefCfaOffset(
                             nullptr, int64_t(MFI.getStackSize())));
      continue;
    }
    if (HasFP && !SeenFP && MI.modifiesRegister(MIPSX::R30, TRI)) {
      SeenFP = true;
      insertCFIAfter(MI, MCCFIInstruction::cfiDefCfa(
                             nullptr, MRI->getDwarfRegNum(MIPSX::R30, true),
                             0));
      continue;
    }
    if (MI.getOpcode() != MIPSX::ST)
      continue;
    Register Reg = MI.getOperand(0).getReg();
    for (const CalleeSavedInfo &CS : MFI.getCalleeSavedInfo()) {
      if (Register(CS.getReg()) != Reg || !Saved.insert(Reg).second)
        continue;
      int64_t Offset = MFI.getObjectOffset(CS.getFrameIdx());
      if (Reg == MIPSX::R31)
        insertCFIAfter(MI, MIPSX::createReturnAddressCFI(nullptr, Offset));
      else
        insertCFIAfter(MI, MCCFIInstruction::createOffset(
                               nullptr, MRI->getDwarfRegNum(Reg, true),
                               Offset));
    }
  }
  return true;
}

FunctionPass *llvm::createMIPSXFrameCFIPass() { return new MIPSXFrameCFI(); }
