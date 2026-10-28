//===-- MIPSXISelLowering.cpp - MIPS-X DAG lowering implementation --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Calls, arguments and returns follow MIPS-X SDK docs/abi.md section 7 (see
// MIPSXCallingConv.td). Direct calls are jspci through a linker-selected
// code base (abi section 5.2).
//
//===----------------------------------------------------------------------===//

#include "MIPSXISelLowering.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSXInstrInfo.h"
#include "MIPSXSelectionDAGInfo.h"
#include "MIPSXSubtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "mipsx-lower"

#define GET_CALLING_CONV_IMPL
#include "MIPSXGenCallingConv.inc"

MIPSXTargetLowering::MIPSXTargetLowering(const TargetMachine &TM,
                                         const MIPSXSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  addRegisterClass(MVT::i32, &MIPSX::GPRRegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(MIPSX::R29);
  setBooleanContents(ZeroOrOneBooleanContent);
  setMinFunctionAlignment(Align(4));
  setPrefFunctionAlignment(Align(4));

  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i32, Custom);

  // No multiply or divide instructions: library calls (work package S12
  // provides the routines).
  for (unsigned Op : {ISD::MUL, ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM})
    setOperationAction(Op, MVT::i32, LibCall);
  for (unsigned Op : {ISD::MULHS, ISD::MULHU, ISD::SMUL_LOHI, ISD::UMUL_LOHI,
                      ISD::SDIVREM, ISD::UDIVREM})
    setOperationAction(Op, MVT::i32, Expand);

  for (unsigned Op : {ISD::ROTL, ISD::ROTR, ISD::BSWAP, ISD::CTPOP, ISD::CTLZ,
                      ISD::CTTZ, ISD::SHL_PARTS, ISD::SRA_PARTS,
                      ISD::SRL_PARTS})
    setOperationAction(Op, MVT::i32, Expand);

  // Sign extension in a register: shift left, then arithmetic shift right.
  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16})
    setOperationAction(ISD::SIGN_EXTEND_INREG, VT, Expand);

  // i1 loads go through i8.
  for (unsigned Ext : {ISD::EXTLOAD, ISD::ZEXTLOAD, ISD::SEXTLOAD})
    setLoadExtAction(Ext, MVT::i32, MVT::i1, Promote);

  // TODO (work package S9): conditional branches, setcc, select, jump
  // tables and variable shifts.
}

SDValue MIPSXTargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::ExternalSymbol:
    return LowerExternalSymbol(Op, DAG);
  default:
    llvm_unreachable("unimplemented operation lowering");
  }
}

// The three-instruction %hi/%lo form of docs/abi.md section 6.2:
//   addi r0,#%hi(sym),rd; lsl rd,rd,#15; addi rd,#%lo(sym),rd
static SDValue buildHiLo(SelectionDAG &DAG, const SDLoc &DL, SDValue Hi,
                         SDValue Lo) {
  SDValue R0 = DAG.getRegister(MIPSX::R0, MVT::i32);
  SDNode *H = DAG.getMachineNode(MIPSX::ADDI, DL, MVT::i32, R0, Hi);
  SDNode *S =
      DAG.getMachineNode(MIPSX::SH, DL, MVT::i32, R0, SDValue(H, 0),
                         DAG.getTargetConstant(32 - 15, DL, MVT::i32));
  return SDValue(
      DAG.getMachineNode(MIPSX::ADDI, DL, MVT::i32, SDValue(S, 0), Lo), 0);
}

SDValue MIPSXTargetLowering::LowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const auto *N = cast<GlobalAddressSDNode>(Op);
  const GlobalValue *GV = N->getGlobal();
  int64_t Offset = N->getOffset();
  return buildHiLo(
      DAG, DL,
      DAG.getTargetGlobalAddress(GV, DL, MVT::i32, Offset, MIPSXII::MO_HI),
      DAG.getTargetGlobalAddress(GV, DL, MVT::i32, Offset, MIPSXII::MO_LO));
}

SDValue MIPSXTargetLowering::LowerExternalSymbol(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();
  return buildHiLo(
      DAG, DL, DAG.getTargetExternalSymbol(Sym, MVT::i32, MIPSXII::MO_HI),
      DAG.getTargetExternalSymbol(Sym, MVT::i32, MIPSXII::MO_LO));
}

SDValue MIPSXTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  if (IsVarArg)
    report_fatal_error("MIPS-X: variadic functions are not supported yet");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_MIPSX);

  for (const CCValAssign &VA : ArgLocs) {
    SDValue ArgValue;
    if (VA.isRegLoc()) {
      Register VReg = RegInfo.createVirtualRegister(&MIPSX::GPRRegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, MVT::i32);
    } else {
      assert(VA.isMemLoc() && "Argument not in a register or on the stack");
      // Incoming stack arguments start at the CFA (sp at entry).
      int FI = MFI.CreateFixedObject(4, VA.getLocMemOffset(),
                                     /*IsImmutable=*/true);
      ArgValue = DAG.getLoad(
          MVT::i32, DL, Chain, DAG.getFrameIndex(FI, MVT::i32),
          MachinePointerInfo::getFixedStack(MF, FI));
    }

    // Narrow values arrive extended to 32 bits by the caller.
    if (VA.getLocInfo() == CCValAssign::SExt)
      ArgValue = DAG.getNode(ISD::AssertSext, DL, MVT::i32, ArgValue,
                             DAG.getValueType(VA.getValVT()));
    else if (VA.getLocInfo() == CCValAssign::ZExt)
      ArgValue = DAG.getNode(ISD::AssertZext, DL, MVT::i32, ArgValue,
                             DAG.getValueType(VA.getValVT()));
    if (VA.getLocInfo() != CCValAssign::Full)
      ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);
    InVals.push_back(ArgValue);
  }
  return Chain;
}

bool MIPSXTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type * /*RetTy*/) const {
  // Larger return values go through a hidden pointer in r4 (sret demotion),
  // as abi section 7.3 says.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_MIPSX);
}

SDValue
MIPSXTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_MIPSX);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);
  for (unsigned I = 0, E = RVLocs.size(); I != E; ++I) {
    CCValAssign &VA = RVLocs[I];
    assert(VA.isRegLoc() && "Can only return in registers");
    SDValue Val = OutVals[I];
    switch (VA.getLocInfo()) {
    case CCValAssign::Full:
      break;
    case CCValAssign::SExt:
      Val = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Val);
      break;
    case CCValAssign::ZExt:
      Val = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Val);
      break;
    case CCValAssign::AExt:
      Val = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Val);
      break;
    default:
      llvm_unreachable("Unknown loc info");
    }
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);
  return DAG.getNode(MIPSXISD::RET_GLUE, DL, MVT::Other, RetOps);
}

SDValue MIPSXTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  MachineFunction &MF = DAG.getMachineFunction();

  // Tail calls arrive in work package S10.
  CLI.IsTailCall = false;

  for (const ISD::OutputArg &Out : Outs)
    if (Out.Flags.isByVal())
      report_fatal_error("MIPS-X: byval arguments are not supported");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_MIPSX);
  unsigned NumBytes = CCInfo.getStackSize();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;
  SDValue StackPtr;
  for (unsigned I = 0, E = ArgLocs.size(); I != E; ++I) {
    CCValAssign &VA = ArgLocs[I];
    SDValue Arg = OutVals[I];
    switch (VA.getLocInfo()) {
    case CCValAssign::Full:
      break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    default:
      llvm_unreachable("Unknown loc info");
    }

    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
      continue;
    }
    // Outgoing stack arguments at sp + offset; sp does not move around
    // calls.
    assert(VA.isMemLoc());
    if (!StackPtr.getNode())
      StackPtr = DAG.getCopyFromReg(Chain, DL, MIPSX::R29, MVT::i32);
    SDValue Addr = DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr,
                               DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));
    MemOpChains.push_back(
        DAG.getStore(Chain, DL, Arg, Addr, MachinePointerInfo()));
  }
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue Glue;
  for (auto &[Reg, Val] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Val, Glue);
    Glue = Chain.getValue(1);
  }

  // TODO (work package S10): indirect calls and -mlong-calls.
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32,
                                        G->getOffset());
  else if (auto *S = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), MVT::i32);
  else
    report_fatal_error("MIPS-X: indirect calls are not supported yet");

  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &[Reg, Val] : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg, Val.getValueType()));
  const uint32_t *Mask =
      Subtarget.getRegisterInfo()->getCallPreservedMask(MF, CallConv);
  Ops.push_back(DAG.getRegisterMask(Mask));
  if (Glue.getNode())
    Ops.push_back(Glue);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(MIPSXISD::CALL, DL, NodeTys, Ops);
  Glue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
  Glue = Chain.getValue(1);

  // Results.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  RetInfo.AnalyzeCallResult(Ins, RetCC_MIPSX);
  for (const CCValAssign &VA : RVLocs) {
    SDValue Val =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue);
    Chain = Val.getValue(1);
    Glue = Val.getValue(2);
    if (VA.getLocInfo() == CCValAssign::SExt)
      Val = DAG.getNode(ISD::AssertSext, DL, MVT::i32, Val,
                        DAG.getValueType(VA.getValVT()));
    else if (VA.getLocInfo() == CCValAssign::ZExt)
      Val = DAG.getNode(ISD::AssertZext, DL, MVT::i32, Val,
                        DAG.getValueType(VA.getValVT()));
    if (VA.getLocInfo() != CCValAssign::Full)
      Val = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), Val);
    InVals.push_back(Val);
  }
  return Chain;
}
