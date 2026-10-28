//===-- MIPSXISelDAGToDAG.cpp - A DAG pattern matching selector for MIPS-X ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSX.h"
#include "MIPSXInstrInfo.h"
#include "MIPSXSelectionDAGInfo.h"
#include "MIPSXSubtarget.h"
#include "MIPSXTargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "mipsx-isel"
#define PASS_NAME "MIPS-X DAG->DAG Pattern Instruction Selection"

namespace {

class MIPSXDAGToDAGISel : public SelectionDAGISel {
  const MIPSXSubtarget *Subtarget = nullptr;

public:
  MIPSXDAGToDAGISel() = delete;
  explicit MIPSXDAGToDAGISel(MIPSXTargetMachine &TM) : SelectionDAGISel(TM) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<MIPSXSubtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

private:
#include "MIPSXGenDAGISel.inc"

  void Select(SDNode *N) override;
  bool selectAddrRegImm(SDValue Addr, SDValue &Base, SDValue &Offset);
  bool selectAddrRegImmNoSym(SDValue Addr, SDValue &Base, SDValue &Offset);
  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override;
};

class MIPSXDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  explicit MIPSXDAGToDAGISelLegacy(MIPSXTargetMachine &TM)
      : SelectionDAGISelLegacy(ID, std::make_unique<MIPSXDAGToDAGISel>(TM)) {}
};

} // end anonymous namespace

char MIPSXDAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(MIPSXDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

// The %lo operand of the last addi of a %hi/%lo address, or an empty value.
static SDValue getLoOperand(SDValue Addr) {
  if (!Addr.isMachineOpcode() || Addr.getMachineOpcode() != MIPSX::ADDI)
    return SDValue();
  SDValue Op = Addr.getOperand(1);
  unsigned Flags;
  if (auto *GA = dyn_cast<GlobalAddressSDNode>(Op))
    Flags = GA->getTargetFlags();
  else if (auto *ES = dyn_cast<ExternalSymbolSDNode>(Op))
    Flags = ES->getTargetFlags();
  else if (auto *JT = dyn_cast<JumpTableSDNode>(Op))
    Flags = JT->getTargetFlags();
  else if (auto *BA = dyn_cast<BlockAddressSDNode>(Op))
    Flags = BA->getTargetFlags();
  else
    return SDValue();
  return Flags == MIPSXII::MO_LO ? Op : SDValue();
}

// Loads and stores take a base register and a 17-bit offset. The %lo part
// of a symbol's address goes in the offset, since it is never negative
// (docs/abi.md section 6.2): addi r0,#%hi(g),r2; lsl r2,r2,#15;
// ld %lo(g)[r2],r3.
bool MIPSXDAGToDAGISel::selectAddrRegImm(SDValue Addr, SDValue &Base,
                                         SDValue &Offset) {
  if (SDValue Lo = getLoOperand(Addr)) {
    Base = Addr.getOperand(0);
    Offset = Lo;
    return true;
  }
  return selectAddrRegImmNoSym(Addr, Base, Offset);
}

// The same with a constant offset only, as inline assembly prints it. A
// frame index base is resolved by eliminateFrameIndex; a constant address
// in the lowest or highest 64 KB uses r0 (docs/abi.md section 6.2).
bool MIPSXDAGToDAGISel::selectAddrRegImmNoSym(SDValue Addr, SDValue &Base,
                                              SDValue &Offset) {
  SDLoc DL(Addr);
  auto AsBase = [&](SDValue V) {
    if (auto *FI = dyn_cast<FrameIndexSDNode>(V))
      return CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
    return V;
  };

  if (auto *C = dyn_cast<ConstantSDNode>(Addr)) {
    int64_t V = C->getSExtValue();
    if (isInt<17>(V)) {
      Base = CurDAG->getRegister(MIPSX::R0, MVT::i32);
      Offset = CurDAG->getSignedTargetConstant(V, DL, MVT::i32);
      return true;
    }
  }

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    int64_t V = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<17>(V)) {
      Base = AsBase(Addr.getOperand(0));
      Offset = CurDAG->getSignedTargetConstant(V, DL, MVT::i32);
      return true;
    }
  }

  Base = AsBase(Addr);
  Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
  return true;
}

// An "m" operand is a base register and an offset, printed as off[rN].
bool MIPSXDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  switch (ConstraintID) {
  case InlineAsm::ConstraintCode::m:
  case InlineAsm::ConstraintCode::o: {
    SDValue Base, Offset;
    selectAddrRegImmNoSym(Op, Base, Offset);
    OutOps.push_back(Base);
    OutOps.push_back(Offset);
    return false;
  }
  default:
    return true;
  }
}

void MIPSXDAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  SDLoc DL(N);
  switch (N->getOpcode()) {
  case ISD::Constant:
    // Zero is r0.
    if (cast<ConstantSDNode>(N)->isZero()) {
      SDValue New = CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL,
                                           MIPSX::R0, MVT::i32);
      ReplaceNode(N, New.getNode());
      return;
    }
    break;
  case ISD::FrameIndex: {
    // The address of a stack object: addi sp,#offset,rd after frame index
    // elimination.
    int FI = cast<FrameIndexSDNode>(N)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i32);
    ReplaceNode(N, CurDAG->getMachineNode(
                       MIPSX::ADDI, DL, MVT::i32, TFI,
                       CurDAG->getTargetConstant(0, DL, MVT::i32)));
    return;
  }
  default:
    break;
  }

  SelectCode(N);
}

FunctionPass *llvm::createMIPSXISelDag(MIPSXTargetMachine &TM) {
  return new MIPSXDAGToDAGISelLegacy(TM);
}
