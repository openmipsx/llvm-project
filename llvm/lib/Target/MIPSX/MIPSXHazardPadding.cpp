//===-- MIPSXHazardPadding.cpp - Pad delay slots and hazards with nops ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MIPS-X has no interlocks (MIPS-X SDK docs/abi.md section 9). This pass,
// the last before emission, makes the code meet the rules after
// MIPSXDelaySlotFiller has done what it can:
//
// - every branch and jump gets exactly two delay-slot instructions in its
//   bundle; missing ones are nops (P3);
// - an instruction that reads the destination of the load just before it
//   (in layout order, across block boundaries) gets a nop in between (P1).
//   The filler never puts a load in a second delay slot, nor a load in the
//   first one read by the second, so no load is in flight at a branch
//   target, a call or a return (section 9.2) and layout order is enough.
//
// P2 (a load must not overwrite its address register) is ensured by an
// early-clobber constraint on load destinations. The code generator does not
// emit the MD instructions (P4).
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSX.h"
#include "MIPSXInstrInfo.h"
#include "MIPSXSubtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "mipsx-hazard-padding"

STATISTIC(NumDelaySlotNops, "Number of nops in delay slots");
STATISTIC(NumLoadNops, "Number of nops after loads");

namespace {

class MIPSXHazardPadding : public MachineFunctionPass {
public:
  static char ID;
  MIPSXHazardPadding() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "MIPS-X hazard padding"; }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // end anonymous namespace

char MIPSXHazardPadding::ID = 0;

INITIALIZE_PASS(MIPSXHazardPadding, DEBUG_TYPE, "MIPS-X hazard padding",
                false, false)

static MachineInstr *insertNop(MachineBasicBlock &MBB,
                               MachineBasicBlock::instr_iterator I,
                               const MIPSXInstrInfo &TII, const DebugLoc &DL) {
  // nop is add r0,r0,r0.
  return BuildMI(MBB, I, DL, TII.get(MIPSX::ADD), MIPSX::R0)
      .addReg(MIPSX::R0)
      .addReg(MIPSX::R0);
}

// The register a load writes, if MI is a load.
static Register loadDest(const MachineInstr &MI) {
  if (!MI.mayLoad() || MI.isCall() || MI.getNumOperands() == 0 ||
      !MI.getOperand(0).isReg() || !MI.getOperand(0).isDef())
    return Register();
  return MI.getOperand(0).getReg();
}

// Whether the hardware reads Reg when it executes MI: its explicit register
// operands, and r31 for a return (jspci r31,#0,r0).
static bool readsReg(const MachineInstr &MI, Register Reg) {
  if (MI.isReturn() && Reg == MIPSX::R31)
    return true;
  for (const MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.isUse() && !MO.isImplicit() && MO.getReg() == Reg)
      return true;
  return false;
}

bool MIPSXHazardPadding::runOnMachineFunction(MachineFunction &MF) {
  const MIPSXInstrInfo &TII = *MF.getSubtarget<MIPSXSubtarget>().getInstrInfo();
  bool Changed = false;

  // Two instructions in every delay slot bundle.
  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.instr_begin(), E = MBB.instr_end(); I != E;) {
      MachineInstr &MI = *I++;
      if (!MI.hasDelaySlot() || MI.isInsideBundle())
        continue;
      unsigned Slots = 0;
      auto End = std::next(MI.getIterator());
      while (End != MBB.instr_end() && End->isInsideBundle()) {
        ++Slots;
        ++End;
      }
      if (Slots > 2)
        report_fatal_error("MIPS-X: more than two delay-slot instructions");
      for (; Slots < 2; ++Slots) {
        MachineInstr *Nop = insertNop(MBB, End, TII, MI.getDebugLoc());
        Nop->bundleWithPred();
        ++NumDelaySlotNops;
        Changed = true;
      }
      I = End;
    }
  }

  // Load-use pairs, in layout order.
  Register Loaded;
  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.instr_begin(), E = MBB.instr_end(); I != E; ++I) {
      MachineInstr &MI = *I;
      if (MI.isMetaInstruction() || MI.isBundle())
        continue;
      if (Loaded && readsReg(MI, Loaded)) {
        if (MI.isInsideBundle())
          report_fatal_error("MIPS-X: load-use hazard inside a delay slot");
        insertNop(MBB, I, TII, MI.getDebugLoc());
        ++NumLoadNops;
        Changed = true;
      }
      Loaded = loadDest(MI);
      if (Loaded && MI.isInsideBundle() &&
          (std::next(I) == E || !std::next(I)->isInsideBundle()))
        report_fatal_error("MIPS-X: load in the second delay slot");
    }
  }
  return Changed;
}

FunctionPass *llvm::createMIPSXHazardPaddingPass() {
  return new MIPSXHazardPadding();
}
