//===-- MIPSXISelLowering.h - MIPS-X DAG lowering interface -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXISELLOWERING_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXISELLOWERING_H

#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class MIPSXSubtarget;

class MIPSXTargetLowering : public TargetLowering {
  const MIPSXSubtarget &Subtarget;

public:
  MIPSXTargetLowering(const TargetMachine &TM, const MIPSXSubtarget &STI);

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  // %hi/%lo relocations carry an addend, so sym+offset is one address.
  bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const override {
    return true;
  }

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  // Jump-table entries are word addresses: .word %w(label) (abi section 5.6).
  unsigned getJumpTableEncoding() const override;
  const MCExpr *LowerCustomJumpTableEntry(const MachineJumpTableInfo *MJTI,
                                          const MachineBasicBlock *MBB,
                                          unsigned UID,
                                          MCContext &Ctx) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  // Inline assembly: "r" is any general register; "I" a signed 17-bit
  // immediate, "J" zero and "K" a shift amount (0 to 31), as in Clang's
  // MIPSXTargetInfo::validateAsmConstraint.
  ConstraintType getConstraintType(StringRef Constraint) const override;
  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;
  void LowerAsmOperandForConstraint(SDValue Op, StringRef Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

private:
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerExternalSymbol(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_JT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerUDIVREM(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerShift(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSXISELLOWERING_H
