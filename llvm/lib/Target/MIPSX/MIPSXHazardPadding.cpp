//===-- MIPSXHazardPadding.cpp - Pad delay slots and hazards with nops ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MIPS-X has no interlocks (MIPS-X SDK docs/abi.md section 9). This pass,
// run just before emission, makes code correct in the simplest way:
//
// - every branch and jump (anything with a delay slot) is followed by two
//   nops, bundled with it (P3; the second slot is never a load, section
//   9.2);
// - every load is followed by a nop, so no instruction reads a loaded
//   register too early, within a block or across a branch, call or return
//   (P1).
//
// P2 (a load must not overwrite its address register) is ensured by an
// early-clobber constraint on load destinations. The code generator does not
// yet emit the MD instructions (P4).
//
// TODO (work package S11): fill the slots with useful instructions and
// schedule loads away from their uses instead.
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

bool MIPSXHazardPadding::runOnMachineFunction(MachineFunction &MF) {
  const MIPSXInstrInfo &TII = *MF.getSubtarget<MIPSXSubtarget>().getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.instr_begin(), E = MBB.instr_end(); I != E;) {
      MachineInstr &MI = *I;
      auto Next = std::next(I);
      if (MI.isMetaInstruction() || MI.isBundled()) {
        I = Next;
        continue;
      }

      if (MI.hasDelaySlot()) {
        insertNop(MBB, Next, TII, MI.getDebugLoc());
        MachineInstr *Last = insertNop(MBB, Next, TII, MI.getDebugLoc());
        MIBundleBuilder(MBB, MI.getIterator(),
                        std::next(Last->getIterator()));
        NumDelaySlotNops += 2;
        Changed = true;
      } else if (MI.mayLoad()) {
        insertNop(MBB, Next, TII, MI.getDebugLoc());
        ++NumLoadNops;
        Changed = true;
      }
      I = Next;
    }
  }
  return Changed;
}

FunctionPass *llvm::createMIPSXHazardPaddingPass() {
  return new MIPSXHazardPadding();
}
