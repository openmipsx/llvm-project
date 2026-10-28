//===-- MIPSXDelaySlotFiller.cpp - Fill MIPS-X delay slots ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Every MIPS-X branch and jump has two delay slots (MIPS-X SDK docs/abi.md
// sections 5 and 9). This pass fills them, bundling the slot instructions
// with the branch; MIPSXHazardPadding then pads what is left with nops.
//
// 1. From above: up to two earlier instructions of the same block that the
//    branch does not depend on, and that do not depend on anything they
//    move past, move into the slots. They always execute, so this works
//    for every branch, jump, call and return.
// 2. From the target: if nothing could move from above, a conditional
//    branch copies the first one or two instructions of its taken target
//    into the slots, becomes the squashing form (the slots execute only if
//    the branch is taken), and is retargeted past the copies; the target
//    block is split there. An unconditional branch does the same without
//    squashing. A loop branch back to its own block is the usual case. If
//    the branch is the only way into the target, the instructions move
//    instead of being copied, which also makes the code smaller.
// 3. From the fall-through: a conditional branch that keeps slots
//    unfilled after (1) and is the only way into the block it falls
//    through to takes that block's first instructions, if they do not
//    store, load only from the stack or small data (an address that is
//    always valid), and set no register live at the taken target, so that
//    executing them on the taken path does no harm.
//
// Slot contents follow section 9.2: no branch, jump or call in a slot, no
// load in the second slot, and a load in the first slot only if the second
// does not read it.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSX.h"
#include "MIPSXInstrInfo.h"
#include "MIPSXSubtarget.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "mipsx-delay-slot-filler"

STATISTIC(NumFilledAbove, "Number of delay slots filled from above");
STATISTIC(NumFilledTarget, "Number of delay slots filled from the target");
STATISTIC(NumMovedTarget, "Number of those moved rather than copied");
STATISTIC(NumFilledFallThrough,
          "Number of delay slots filled from the fall-through");

static cl::opt<bool>
    DisableFiller("disable-mipsx-delay-filler", cl::Hidden, cl::init(false),
                  cl::desc("Leave MIPS-X delay slots to the nop padding"));

static cl::opt<bool>
    DisableSquash("disable-mipsx-squash-fill", cl::Hidden, cl::init(false),
                  cl::desc("Do not fill MIPS-X delay slots from branch "
                           "targets"));

static cl::opt<bool> DisableFallThrough(
    "disable-mipsx-fallthrough-fill", cl::Hidden, cl::init(false),
    cl::desc("Do not fill MIPS-X delay slots from fall-through blocks"));

namespace {

class MIPSXDelaySlotFiller : public MachineFunctionPass {
  const MIPSXInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

public:
  static char ID;
  MIPSXDelaySlotFiller() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "MIPS-X delay slot filler";
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  unsigned fillFromAbove(MachineBasicBlock &MBB, MachineInstr &Br,
                         bool DryRun);
  unsigned fillFromTarget(MachineBasicBlock &MBB, MachineInstr &Br,
                          bool DryRun);
  unsigned fillFromFallThrough(MachineBasicBlock &MBB, MachineInstr &Br);
};

// Registers an instruction reads and writes, including implicit operands; a
// return also reads r31.
struct RegSets {
  SmallSet<Register, 8> Uses, Defs;

  void add(const MachineInstr &MI) {
    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.getReg())
        continue;
      if (MO.isDef())
        Defs.insert(MO.getReg());
      else
        Uses.insert(MO.getReg());
    }
    if (MI.isReturn())
      Uses.insert(MIPSX::R31);
  }
};

} // end anonymous namespace

char MIPSXDelaySlotFiller::ID = 0;

INITIALIZE_PASS(MIPSXDelaySlotFiller, DEBUG_TYPE, "MIPS-X delay slot filler",
                false, false)

static bool isLoad(const MachineInstr &MI) {
  return MI.mayLoad() && !MI.isCall();
}

static Register loadDest(const MachineInstr &MI) {
  return isLoad(MI) ? MI.getOperand(0).getReg() : Register();
}

static bool readsReg(const MachineInstr &MI, Register Reg) {
  for (const MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.isUse() && MO.getReg() == Reg)
      return true;
  return false;
}

// An instruction that may sit in a delay slot: an ordinary instruction with
// no control flow, side effects or bundle.
static bool canBeInSlot(const MachineInstr &MI) {
  if (MI.isDebugInstr() || MI.isMetaInstruction() || MI.isBundled() ||
      MI.hasDelaySlot() || MI.isCall() || MI.isBranch() || MI.isReturn() ||
      MI.isTerminator() || MI.isInlineAsm() || MI.isPseudo() ||
      MI.hasUnmodeledSideEffects() || MI.isPosition())
    return false;
  // Volatile and atomic accesses keep their place.
  for (const MachineMemOperand *MMO : MI.memoperands())
    if (MMO->isVolatile() || MMO->isAtomic())
      return false;
  return MI.getDesc().getSize() == 4;
}

// Whether a pair of slot instructions (Slot1 then Slot2) obeys section 9.2.
static bool slotsOK(const MachineInstr *Slot1, const MachineInstr *Slot2) {
  if (Slot2 && isLoad(*Slot2))
    return false;
  if (Slot1 && Slot2 && isLoad(*Slot1) && readsReg(*Slot2, loadDest(*Slot1)))
    return false;
  return true;
}

// The registers the branch or jump itself reads and writes. Implicit operands
// of calls and returns (argument and result registers, the stack pointer)
// belong to the callee or caller, which run after the slots, so an
// instruction setting them may move into the slots. A call writes r31 and a
// return reads it.
static void addBranchRegs(RegSets &Sets, const MachineInstr &Br) {
  for (const MachineOperand &MO : Br.operands()) {
    if (!MO.isReg() || !MO.getReg() || MO.isImplicit())
      continue;
    if (MO.isDef())
      Sets.Defs.insert(MO.getReg());
    else
      Sets.Uses.insert(MO.getReg());
  }
  if (Br.isCall())
    Sets.Defs.insert(MIPSX::R31);
  if (Br.isReturn())
    Sets.Uses.insert(MIPSX::R31);
}

// Keeps register liveness right for the verifier once the slots are bundled
// after Br. Within a bundle, a read of a register set by an earlier member is
// an internal read. The slots execute before the callee, return address or
// branch target does, so the branch's implicit reads of registers a slot sets
// (a call's arguments, a return's result) are internal reads too. And a
// register that Br or a slot kills is not dead there if a later slot reads
// it.
static void fixSlotLiveness(MachineInstr &Br, const TargetRegisterInfo *TRI) {
  SmallVector<MachineInstr *, 2> Slots;
  for (auto I = std::next(Br.getIterator()), E = Br.getParent()->instr_end();
       I != E && I->isBundledWithPred(); ++I)
    Slots.push_back(&*I);
  for (unsigned N = 0; N != Slots.size(); ++N) {
    MachineInstr &Slot = *Slots[N];
    for (MachineOperand &MO : Br.operands()) {
      if (!MO.isReg() || !MO.isUse() || !MO.getReg())
        continue;
      if (MO.isImplicit() && Slot.modifiesRegister(MO.getReg(), TRI))
        MO.setIsInternalRead();
      else if (MO.isKill() && Slot.readsRegister(MO.getReg(), TRI))
        MO.setIsKill(false);
    }
    if (N == 0)
      continue;
    MachineInstr &Prev = *Slots[N - 1];
    for (MachineOperand &MO : Slot.operands()) {
      if (!MO.isReg() || !MO.isUse() || !MO.getReg())
        continue;
      if (Prev.modifiesRegister(MO.getReg(), TRI))
        MO.setIsInternalRead();
      else if (MO.readsReg())
        Prev.clearRegisterKills(MO.getReg(), TRI);
    }
  }
}

// Returns the number of slots filled (or, with DryRun, that would be).
unsigned MIPSXDelaySlotFiller::fillFromAbove(MachineBasicBlock &MBB,
                                             MachineInstr &Br, bool DryRun) {
  RegSets Blocked; // Registers of the branch and of skipped instructions.
  addBranchRegs(Blocked, Br);
  bool SkippedLoad = false, SkippedStore = false;
  SmallVector<MachineInstr *, 2> Picked; // Nearest the branch first.

  unsigned Budget = 16;
  for (auto I = Br.getReverseIterator(); ++I != MBB.instr_rend();) {
    MachineInstr &MI = *I;
    if (MI.isDebugInstr())
      continue;
    // Nothing moves across a barrier (MEMBARRIER, from a fence).
    if (MI.isBundled() || MI.hasDelaySlot() || MI.isInlineAsm() ||
        MI.isPosition() || MI.hasUnmodeledSideEffects() || --Budget == 0)
      break;

    bool Movable = canBeInSlot(MI);
    if (Movable) {
      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.getReg())
          continue;
        Register R = MO.getReg();
        if (MO.isDef() &&
            (Blocked.Uses.count(R) || Blocked.Defs.count(R)))
          Movable = false;
        if (MO.isUse() && Blocked.Defs.count(R))
          Movable = false;
      }
      if (MI.mayStore() && (SkippedLoad || SkippedStore))
        Movable = false;
      if (MI.mayLoad() && SkippedStore)
        Movable = false;
    }
    // With two, the nearer instruction (picked first) is the second slot.
    if (Movable && Picked.size() == 1 && !slotsOK(&MI, Picked[0]))
      Movable = false;
    if (Movable) {
      // Picked instructions keep their order, so they do not block each
      // other.
      Picked.push_back(&MI);
      if (Picked.size() == 2)
        break;
      continue;
    }
    // A skipped instruction blocks what lies above it.
    Blocked.add(MI);
    SkippedLoad |= MI.mayLoad();
    SkippedStore |= MI.mayStore();
  }

  if (Picked.empty() || DryRun)
    return Picked.size();
  // A single instruction goes in the first slot; with two, the earlier one
  // goes first. Either way a lone load is fine (the second slot is a nop).
  // An instruction moved down now reads its registers after the ones it
  // moved past (and the branch), so their kill flags go.
  for (MachineInstr *MI : Picked)
    for (const MachineOperand &Use : MI->uses())
      if (Use.isReg() && Use.getReg() && Use.readsReg())
        for (auto I = std::next(MI->getIterator()); &*I != &Br; ++I)
          I->clearRegisterKills(Use.getReg(), TRI);
  MachineBasicBlock::instr_iterator After = std::next(Br.getIterator());
  for (MachineInstr *MI : reverse(Picked)) {
    MI->removeFromParent();
    MBB.insert(After, MI);
    MI->bundleWithPred();
    ++NumFilledAbove;
  }
  fixSlotLiveness(Br, TRI);
  return Picked.size();
}

static unsigned getSquashOpcode(unsigned Opc) {
  switch (Opc) {
  case MIPSX::BEQ:
    return MIPSX::BEQSQ;
  case MIPSX::BNE:
    return MIPSX::BNESQ;
  case MIPSX::BLT:
    return MIPSX::BLTSQ;
  case MIPSX::BGE:
    return MIPSX::BGESQ;
  case MIPSX::BLO:
    return MIPSX::BLOSQ;
  case MIPSX::BHS:
    return MIPSX::BHSSQ;
  default:
    return 0;
  }
}

// Whether Br, in MBB, is the only way into Dest: Dest has no other
// predecessor, no other instruction of MBB refers to it, and MBB does not
// fall through to it. With a null Br, whether falling through from MBB is
// the only way into Dest.
static bool isOnlyEntry(MachineBasicBlock &MBB, const MachineInstr *Br,
                        MachineBasicBlock &Dest) {
  if (&Dest == &MBB || Dest.pred_size() != 1 || *Dest.pred_begin() != &MBB ||
      Dest.isEHPad() || Dest.hasAddressTaken() ||
      Dest.isInlineAsmBrIndirectTarget())
    return false;
  for (const MachineInstr &MI : MBB.instrs())
    if (&MI != Br)
      for (const MachineOperand &MO : MI.operands())
        if (MO.isMBB() && MO.getMBB() == &Dest)
          return false;
  if (!Br)
    return true;
  MachineBasicBlock::const_iterator Last = MBB.getLastNonDebugInstr();
  return MBB.getNextNode() != &Dest ||
         (Last != MBB.end() && Last->isBarrier());
}

unsigned MIPSXDelaySlotFiller::fillFromTarget(MachineBasicBlock &MBB,
                                              MachineInstr &Br, bool DryRun) {
  unsigned Opc = Br.getOpcode();
  unsigned SqOpc = Opc == MIPSX::B ? MIPSX::B : getSquashOpcode(Opc);
  if (!SqOpc)
    return 0;
  MachineOperand &Dst = Br.getOperand(Opc == MIPSX::B ? 0 : 2);
  MachineBasicBlock *Target = Dst.getMBB();
  if (Target->isEHPad() || Target->hasAddressTaken() ||
      Target->isInlineAsmBrIndirectTarget())
    return 0;
  // A conditional branch whose fall-through is also its target: leave it.
  if (Opc != MIPSX::B && MBB.getNextNode() == Target)
    return 0;

  // The first one or two instructions of the target.
  SmallVector<MachineInstr *, 2> Copy;
  for (MachineInstr &MI : Target->instrs()) {
    if (MI.isDebugInstr())
      continue;
    if (!canBeInSlot(MI))
      break;
    Copy.push_back(&MI);
    if (Copy.size() == 2)
      break;
  }
  if (Copy.size() == 2 && !slotsOK(Copy[0], Copy[1]))
    Copy.pop_back();
  // Copying a branch's own operands is fine: the branch reads them first.
  if (Copy.empty())
    return 0;

  // If Br is the only way into the target, move the instructions: the
  // target keeps its label and needs no split.
  MachineFunction &MF = *MBB.getParent();
  if (isOnlyEntry(MBB, &Br, *Target)) {
    if (DryRun)
      return Copy.size();
    Br.setDesc(TII->get(SqOpc));
    MachineBasicBlock::instr_iterator After = std::next(Br.getIterator());
    for (MachineInstr *MI : Copy) {
      MI->removeFromParent();
      // A squashed slot's registers may still be needed on the
      // fall-through path.
      for (MachineOperand &MO : MI->operands())
        if (MO.isReg() && MO.isUse())
          MO.setIsKill(false);
      MBB.insert(After, MI);
      MI->bundleWithPred();
      ++NumFilledTarget;
      ++NumMovedTarget;
    }
    fixSlotLiveness(Br, TRI);
    fullyRecomputeLiveIns({Target});
    return Copy.size();
  }

  // The new branch target is what follows the copies: the rest of the
  // target block, split off, or if nothing is left the block it falls
  // through to.
  MachineBasicBlock::iterator SplitAt = std::next(Copy.back()->getIterator());
  while (SplitAt != Target->end() && SplitAt->isDebugInstr())
    ++SplitAt;
  MachineBasicBlock *Rest;
  if (SplitAt == Target->end()) {
    Rest = Target->getNextNode();
    if (!Rest || Target->succ_size() != 1 || !Target->isSuccessor(Rest) ||
        Rest == &MBB)
      return 0;
    if (DryRun)
      return Copy.size();
  } else {
    if (DryRun)
      return Copy.size();
    Rest = MF.CreateMachineBasicBlock(Target->getBasicBlock());
    MF.insert(std::next(Target->getIterator()), Rest);
    Rest->splice(Rest->begin(), Target, SplitAt, Target->end());
    Rest->transferSuccessorsAndUpdatePHIs(Target);
    Target->addSuccessor(Rest);
    recomputeLivenessFlags(*Rest);
    LivePhysRegs LiveRegs;
    computeAndAddLiveIns(LiveRegs, *Rest);
  }

  // The branch block now goes to Rest (Br may itself be in Rest if it
  // branched to its own block). It still goes to Target if another branch
  // in it (a conditional branch and an unconditional one can share a
  // target) or its fall-through does.
  MachineBasicBlock &BrMBB = *Br.getParent();
  Dst.setMBB(Rest);
  Br.setDesc(TII->get(SqOpc));
  bool StillToTarget = false;
  for (MachineInstr &MI : BrMBB.instrs())
    for (const MachineOperand &MO : MI.operands())
      if (MO.isMBB() && MO.getMBB() == Target)
        StillToTarget = true;
  MachineBasicBlock::iterator Last = BrMBB.getLastNonDebugInstr();
  if (BrMBB.getNextNode() == Target &&
      (Last == BrMBB.end() || !Last->isBarrier()))
    StillToTarget = true;
  if (BrMBB.isSuccessor(Target) && !StillToTarget) {
    if (BrMBB.isSuccessor(Rest))
      BrMBB.removeSuccessor(Target);
    else
      BrMBB.replaceSuccessor(Target, Rest);
  }
  if (!BrMBB.isSuccessor(Rest))
    BrMBB.addSuccessor(Rest);

  MachineBasicBlock::instr_iterator After = std::next(Br.getIterator());
  for (MachineInstr *MI : Copy) {
    MachineInstr *Dup = MF.CloneMachineInstr(MI);
    // The copies' registers may still be needed on the fall-through path.
    for (MachineOperand &MO : Dup->operands())
      if (MO.isReg() && MO.isUse())
        MO.setIsKill(false);
    BrMBB.insert(After, Dup);
    Dup->bundleWithPred();
    ++NumFilledTarget;
  }
  fixSlotLiveness(Br, TRI);
  return Copy.size();
}

// Br is a conditional branch that ends MBB, with N < 2 of its slots filled
// from above. Returns the number of slots it adds.
unsigned MIPSXDelaySlotFiller::fillFromFallThrough(MachineBasicBlock &MBB,
                                                   MachineInstr &Br) {
  if (!Br.isConditionalBranch() || getSquashOpcode(Br.getOpcode()) == 0)
    return 0;
  MachineBasicBlock *Target = Br.getOperand(2).getMBB();
  MachineBasicBlock *FT = MBB.getNextNode();
  if (!FT || FT == Target || &*MBB.getLastNonDebugInstr() != &Br ||
      !MBB.isSuccessor(FT) || !isOnlyEntry(MBB, nullptr, *FT))
    return 0;
  SmallVector<MachineInstr *, 2> Slots;
  for (auto I = std::next(Br.getIterator()), E = MBB.instr_end();
       I != E && I->isBundledWithPred(); ++I)
    Slots.push_back(&*I);
  if (Slots.size() >= 2)
    return 0;

  SmallVector<MachineInstr *, 2> Moved;
  for (MachineInstr &MI : FT->instrs()) {
    if (MI.isDebugInstr())
      continue;
    if (Slots.size() + Moved.size() == 2 || !canBeInSlot(MI) ||
        MI.mayStore())
      break;
    if (MI.mayLoad()) {
      Register Base = MI.getOperand(1).getReg();
      if (Base != MIPSX::R29 && Base != MIPSX::R30 && Base != MIPSX::R28)
        break;
    }
    bool LiveAtTarget = false;
    for (const MachineOperand &MO : MI.operands())
      if (MO.isReg() && MO.isDef() && MO.getReg())
        for (MCRegAliasIterator A(MO.getReg(), TRI, true); A.isValid(); ++A)
          LiveAtTarget |= Target->isLiveIn(*A);
    if (LiveAtTarget)
      break;
    const MachineInstr *Prev =
        !Moved.empty() ? Moved.back() : (!Slots.empty() ? Slots.back() : nullptr);
    if (Prev && !slotsOK(Prev, &MI))
      break;
    Moved.push_back(&MI);
  }
  if (Moved.empty())
    return 0;
  MachineBasicBlock::instr_iterator After =
      std::next((Slots.empty() ? &Br : Slots.back())->getIterator());
  for (MachineInstr *MI : Moved) {
    MI->removeFromParent();
    // The taken path may still need the registers it reads.
    for (MachineOperand &MO : MI->operands())
      if (MO.isReg() && MO.isUse())
        MO.setIsKill(false);
    MBB.insert(After, MI);
    MI->bundleWithPred();
    ++NumFilledFallThrough;
  }
  fixSlotLiveness(Br, TRI);
  fullyRecomputeLiveIns({FT});
  return Moved.size();
}

bool MIPSXDelaySlotFiller::runOnMachineFunction(MachineFunction &MF) {
  if (DisableFiller)
    return false;
  TII = MF.getSubtarget<MIPSXSubtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();

  // Collect first: filling from a target splits blocks.
  SmallVector<MachineInstr *, 32> Branches;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB.instrs())
      if (MI.hasDelaySlot() && !MI.isBundled())
        Branches.push_back(&MI);

  // Instructions from above always execute, so they are preferred. A
  // backward conditional branch (a loop) is usually taken, so it fills from
  // its target when that fills more slots.
  bool Changed = false;
  for (MachineInstr *Br : Branches) {
    MachineBasicBlock &MBB = *Br->getParent();
    unsigned Above = fillFromAbove(MBB, *Br, /*DryRun=*/true);
    bool Backward = false;
    if (Br->isConditionalBranch()) {
      MachineBasicBlock *T = Br->getOperand(2).getMBB();
      Backward = T->getNumber() <= MBB.getNumber();
    }
    if (!DisableSquash && Above < 2 && (Above == 0 || Backward) &&
        fillFromTarget(MBB, *Br, /*DryRun=*/true) > Above &&
        fillFromTarget(MBB, *Br, /*DryRun=*/false)) {
      Changed = true;
      continue;
    }
    if (Above && fillFromAbove(MBB, *Br, /*DryRun=*/false))
      Changed = true;
    if (Above < 2 && !DisableFallThrough && fillFromFallThrough(MBB, *Br))
      Changed = true;
  }
  return Changed;
}

FunctionPass *llvm::createMIPSXDelaySlotFillerPass() {
  return new MIPSXDelaySlotFiller();
}
