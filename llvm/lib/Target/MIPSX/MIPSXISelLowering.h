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

  // %hi/%lo relocations carry an addend, so sym+offset is one address.
  bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const override {
    return true;
  }

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

private:
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerExternalSymbol(SDValue Op, SelectionDAG &DAG) const;

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
