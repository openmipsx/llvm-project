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
#include "MCTargetDesc/MIPSXMCAsmInfo.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSXInstrInfo.h"
#include "MIPSXMachineFunctionInfo.h"
#include "MIPSXSelectionDAGInfo.h"
#include "MIPSXSubtarget.h"
#include "MIPSXTargetObjectFile.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "mipsx-lower"

#define GET_CALLING_CONV_IMPL
#include "MIPSXGenCallingConv.inc"

static cl::opt<unsigned> MulConstMaxInsts(
    "mipsx-mul-const-max-insts", cl::Hidden, cl::init(6),
    cl::desc("Most instructions a multiplication by a constant may expand to "
             "before it calls __mulsi3 instead"));

static cl::opt<unsigned> MulInlineMaxBits(
    "mipsx-mul-inline-max-bits", cl::Hidden, cl::init(17),
    cl::desc("Widest multiplier (in bits, signed) for which a multiplication "
             "is done inline with mstep instead of calling __mulsi3"));

MIPSXTargetLowering::MIPSXTargetLowering(const TargetMachine &TM,
                                         const MIPSXSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  addRegisterClass(MVT::i32, &MIPSX::GPRRegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(MIPSX::R29);
  setBooleanContents(ZeroOrOneBooleanContent);
  setMinFunctionAlignment(Align(4));
  setPrefFunctionAlignment(Align(4));

  // abi.md section 13.6: no atomic instructions. AtomicExpand turns every
  // atomic operation into an __atomic_* call into the SDK; fences become
  // MEMBARRIER, a compiler barrier (one CPU, in-order memory).
  setMaxAtomicSizeInBitsSupported(0);

  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i32, Custom);
  setOperationAction(ISD::JumpTable, MVT::i32, Custom);
  setOperationAction(ISD::BlockAddress, MVT::i32, Custom);
  setOperationAction(ISD::FRAMEADDR, MVT::i32, Custom);
  setOperationAction(ISD::RETURNADDR, MVT::i32, Custom);

  // Control flow: the six branch conditions compare two registers. setcc
  // and select become select_cc, which is a branch diamond.
  setOperationAction(ISD::BR_CC, MVT::i32, Legal);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::SETCC, MVT::i32, Expand);
  setOperationAction(ISD::SELECT, MVT::i32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Custom);

  // Dynamic allocas: sp moves, and calls then adjust sp around themselves.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32, Expand);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  // Variadic arguments are on the stack; va_list is a char * (abi section
  // 7.2).
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Custom);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setMinStackArgumentAlignment(Align(4));

  // Shifts by a constant are sh and asr; variable shifts call the SDK
  // helpers __ashlsi3, __lshrsi3 and __ashrsi3.
  for (unsigned Op : {ISD::SHL, ISD::SRL, ISD::SRA})
    setOperationAction(Op, MVT::i32, Custom);

  // No multiply or divide instructions: library calls (work package S12
  // provides the routines).
  for (unsigned Op : {ISD::MUL, ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM})
    setOperationAction(Op, MVT::i32, LibCall);
  for (unsigned Op : {ISD::MULHS, ISD::MULHU, ISD::SMUL_LOHI, ISD::UMUL_LOHI,
                      ISD::SDIVREM})
    setOperationAction(Op, MVT::i32, Expand);
  // __udivsi3 also leaves the remainder in r3 (mipsx-sdk
  // rt/builtins/README.md), so a quotient and remainder are one call.
  setOperationAction(ISD::UDIVREM, MVT::i32, Custom);
  // Multiplication by a constant becomes shifts and adds where that is
  // short.
  setTargetDAGCombine(ISD::MUL);

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

}

SDValue MIPSXTargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::ExternalSymbol:
    return LowerExternalSymbol(Op, DAG);
  case ISD::JumpTable:
    return LowerJumpTable(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  case ISD::BR_JT:
    return LowerBR_JT(Op, DAG);
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::VAARG:
    return LowerVAARG(Op, DAG);
  case ISD::UDIVREM:
    return LowerUDIVREM(Op, DAG);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return LowerShift(Op, DAG);
  case ISD::FRAMEADDR:
    return LowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return LowerRETURNADDR(Op, DAG);
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
  // Small data: addi gp,#%gprel(sym),rd (abi section 6.3). Only for an
  // address inside the variable: gp reaches the small data sections, and
  // a[i - 1] or a far offset may lie outside them.
  const auto &TLOF = static_cast<const MIPSXTargetObjectFile &>(
      *getTargetMachine().getObjFileLowering());
  const auto *GVar = dyn_cast<GlobalVariable>(GV);
  if (GVar && GVar->getValueType()->isSized() && Offset >= 0 &&
      uint64_t(Offset) <
          DAG.getDataLayout().getTypeAllocSize(GVar->getValueType()) &&
      TLOF.isGlobalInSmallSection(GVar, getTargetMachine()))
    return SDValue(
        DAG.getMachineNode(MIPSX::ADDI, DL, MVT::i32,
                           DAG.getRegister(MIPSX::R28, MVT::i32),
                           DAG.getTargetGlobalAddress(GV, DL, MVT::i32, Offset,
                                                      MIPSXII::MO_GPREL)),
        0);
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

SDValue MIPSXTargetLowering::LowerJumpTable(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDLoc DL(Op);
  int JTI = cast<JumpTableSDNode>(Op)->getIndex();
  return buildHiLo(DAG, DL,
                   DAG.getTargetJumpTable(JTI, MVT::i32, MIPSXII::MO_HI),
                   DAG.getTargetJumpTable(JTI, MVT::i32, MIPSXII::MO_LO));
}

SDValue MIPSXTargetLowering::LowerBlockAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const auto *N = cast<BlockAddressSDNode>(Op);
  const BlockAddress *BA = N->getBlockAddress();
  int64_t Offset = N->getOffset();
  return buildHiLo(
      DAG, DL,
      DAG.getTargetBlockAddress(BA, MVT::i32, Offset, MIPSXII::MO_HI),
      DAG.getTargetBlockAddress(BA, MVT::i32, Offset, MIPSXII::MO_LO));
}

// Load the word-address entry and jump to it, with no shift (abi section
// 5.6).
SDValue MIPSXTargetLowering::LowerBR_JT(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue Table = Op.getOperand(1);
  SDValue Index = Op.getOperand(2);
  SDValue Offset = DAG.getNode(ISD::SHL, DL, MVT::i32, Index,
                               DAG.getConstant(2, DL, MVT::i32));
  SDValue Addr = DAG.getNode(ISD::ADD, DL, MVT::i32, Table, Offset);
  SDValue Target = DAG.getLoad(
      MVT::i32, DL, Chain, Addr,
      MachinePointerInfo::getJumpTable(DAG.getMachineFunction()));
  return DAG.getNode(MIPSXISD::BRIND_W, DL, MVT::Other, Target.getValue(1),
                     Target);
}

// va_start stores the address of the first variadic word.
SDValue MIPSXTargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  auto *FuncInfo = MF.getInfo<MIPSXMachineFunctionInfo>();
  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), MVT::i32);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

// va_arg reads the next 4-byte word, whatever the alignment of the type: an
// i64 or double is two consecutive words with no padding (abi section 7.2).
// The type legaliser has already split 64-bit types into words.
SDValue MIPSXTargetLowering::LowerVAARG(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue ListPtr = Op.getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  EVT VT = Op.getValueType();
  assert(VT == MVT::i32 && "Unexpected va_arg type");

  SDValue Ap = DAG.getLoad(MVT::i32, DL, Chain, ListPtr, MachinePointerInfo(SV));
  SDValue Next = DAG.getNode(ISD::ADD, DL, MVT::i32, Ap,
                             DAG.getConstant(4, DL, MVT::i32));
  Chain = DAG.getStore(Ap.getValue(1), DL, Next, ListPtr,
                       MachinePointerInfo(SV));
  return DAG.getLoad(VT, DL, Chain, Ap, MachinePointerInfo());
}

// One call to __udivsi3, which returns the quotient in r2 and the remainder
// in r3: the same registers as a returned { i32, i32 }.
// __builtin_frame_address: fp is the CFA, and the caller's fp is saved at
// fp - 8 (MIPSXFrameLowering.cpp), so outer frames follow that chain.
SDValue MIPSXTargetLowering::LowerFRAMEADDR(SDValue Op,
                                            SelectionDAG &DAG) const {
  MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();
  MFI.setFrameAddressIsTaken(true);
  SDLoc DL(Op);
  SDValue FrameAddr =
      DAG.getCopyFromReg(DAG.getEntryNode(), DL, MIPSX::R30, MVT::i32);
  for (unsigned Depth = Op.getConstantOperandVal(0); Depth; --Depth) {
    SDValue Ptr = DAG.getNode(ISD::ADD, DL, MVT::i32, FrameAddr,
                              DAG.getSignedConstant(-8, DL, MVT::i32));
    FrameAddr =
        DAG.getLoad(MVT::i32, DL, DAG.getEntryNode(), Ptr, MachinePointerInfo());
  }
  return FrameAddr;
}

// __builtin_return_address: r31 holds a word address (abi section 5.1); the
// result is the byte address. Outer frames saved theirs at fp - 4.
SDValue MIPSXTargetLowering::LowerRETURNADDR(SDValue Op,
                                             SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MF.getFrameInfo().setReturnAddressIsTaken(true);
  SDLoc DL(Op);
  SDValue RA;
  if (unsigned Depth = Op.getConstantOperandVal(0)) {
    SDValue FrameAddr = LowerFRAMEADDR(
        DAG.getNode(ISD::FRAMEADDR, DL, MVT::i32,
                    DAG.getTargetConstant(Depth, DL, MVT::i32)),
        DAG);
    SDValue Ptr = DAG.getNode(ISD::ADD, DL, MVT::i32, FrameAddr,
                              DAG.getSignedConstant(-4, DL, MVT::i32));
    RA = DAG.getLoad(MVT::i32, DL, DAG.getEntryNode(), Ptr,
                     MachinePointerInfo());
  } else {
    Register Reg = MF.addLiveIn(MIPSX::R31, &MIPSX::GPRRegClass);
    RA = DAG.getCopyFromReg(DAG.getEntryNode(), DL, Reg, MVT::i32);
  }
  return DAG.getNode(ISD::SHL, DL, MVT::i32, RA,
                     DAG.getConstant(2, DL, MVT::i32));
}

SDValue MIPSXTargetLowering::LowerUDIVREM(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LLVMContext &Ctx = *DAG.getContext();
  Type *I32 = Type::getInt32Ty(Ctx);
  ArgListTy Args;
  Args.emplace_back(Op.getOperand(0), I32);
  Args.emplace_back(Op.getOperand(1), I32);
  CallLoweringInfo CLI(DAG);
  CLI.setDebugLoc(DL)
      .setChain(DAG.getEntryNode())
      .setLibCallee(CallingConv::C, StructType::get(Ctx, {I32, I32}),
                    DAG.getExternalSymbol("__udivsi3", MVT::i32),
                    std::move(Args));
  return LowerCallTo(CLI).first;
}

// x * y where known bits show that one operand fits in n <= 17 bits
// (signed): inline as MULSTEPS, mstart and n - 1 msteps with the multiplier
// in the top n bits of MD. That is n + 3 cycles with the shift, against 14 to
// 48 in __mulsi3 plus the call, and the caller need not save r31 for it.
// It is n + 2 words against about four for the call, so not at -Oz.
static SDValue combineMulSteps(SDNode *N, SelectionDAG &DAG) {
  if (DAG.getMachineFunction().getFunction().hasMinSize())
    return SDValue();
  SDValue A = N->getOperand(0), B = N->getOperand(1);
  unsigned BitsA = 33 - DAG.ComputeNumSignBits(A);
  unsigned BitsB = 33 - DAG.ComputeNumSignBits(B);
  if (BitsA < BitsB) {
    std::swap(A, B);
    std::swap(BitsA, BitsB);
  }
  if (BitsB > MulInlineMaxBits)
    return SDValue();
  SDLoc DL(N);
  SDValue MD = DAG.getNode(ISD::SHL, DL, MVT::i32, B,
                           DAG.getConstant(32 - BitsB, DL, MVT::i32));
  return DAG.getNode(MIPSXISD::MULSTEPS, DL, MVT::i32, A, MD,
                     DAG.getTargetConstant(BitsB, DL, MVT::i32));
}

// x * C as a sum of shifted copies of x, using the non-adjacent form of C
// (digits -1, 0, 1, no two adjacent non-zero), if that takes few
// instructions; otherwise mstep inline if an operand is narrow, otherwise
// leave it to __mulsi3.
SDValue MIPSXTargetLowering::PerformDAGCombine(SDNode *N,
                                               DAGCombinerInfo &DCI) const {
  if (N->getOpcode() != ISD::MUL || N->getValueType(0) != MVT::i32 ||
      !DCI.isBeforeLegalizeOps())
    return SDValue();
  auto *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!C)
    return combineMulSteps(N, DCI.DAG);
  int64_t V = C->getSExtValue();
  if (V == 0 || V == 1 || isPowerOf2_64(V))
    return SDValue(); // The generic combines handle these.

  // Non-adjacent form, modulo 2^32.
  SmallVector<std::pair<unsigned, int>, 8> Terms;
  for (int64_t R = V, K = 0; R != 0 && K < 32; R >>= 1, ++K) {
    if (R & 1) {
      int D = (R & 3) == 1 ? 1 : -1;
      R -= D;
      Terms.push_back({unsigned(K), D});
    }
  }
  // Put a positive term first, so that no negation is needed.
  auto Pos = llvm::find_if(Terms, [](auto &T) { return T.second > 0; });
  if (Pos != Terms.end())
    std::swap(*Pos, Terms[0]);
  unsigned Insts = Terms.size() - 1; // Adds and subtracts.
  for (auto &T : Terms)
    Insts += T.first != 0; // Shifts.
  Insts += Terms[0].second < 0; // Negation.
  if (Insts > MulConstMaxInsts)
    return combineMulSteps(N, DCI.DAG);

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  SDValue X = N->getOperand(0);
  auto Shifted = [&](unsigned K) {
    return K == 0 ? X
                  : DAG.getNode(ISD::SHL, DL, MVT::i32, X,
                                DAG.getConstant(K, DL, MVT::i32));
  };
  SDValue Res = Shifted(Terms[0].first);
  if (Terms[0].second < 0)
    Res = DAG.getNode(ISD::SUB, DL, MVT::i32, DAG.getConstant(0, DL, MVT::i32),
                      Res);
  for (auto &T : drop_begin(Terms))
    Res = DAG.getNode(T.second > 0 ? ISD::ADD : ISD::SUB, DL, MVT::i32, Res,
                      Shifted(T.first));
  return Res;
}

TargetLowering::ConstraintType
MIPSXTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return C_RegisterClass;
    case 'I':
    case 'J':
    case 'K':
      return C_Immediate;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
MIPSXTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                  StringRef Constraint,
                                                  MVT VT) const {
  if (Constraint == "r")
    return std::make_pair(0U, &MIPSX::GPRRegClass);
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void MIPSXTargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  if (Constraint.size() == 1) {
    auto *C = dyn_cast<ConstantSDNode>(Op);
    bool Valid = false;
    switch (Constraint[0]) {
    case 'I':
      Valid = C && isInt<17>(C->getSExtValue());
      break;
    case 'J':
      Valid = C && C->isZero();
      break;
    case 'K':
      Valid = C && C->getZExtValue() < 32;
      break;
    default:
      return TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops,
                                                          DAG);
    }
    if (Valid)
      Ops.push_back(DAG.getSignedTargetConstant(C->getSExtValue(), SDLoc(Op),
                                                Op.getValueType()));
    return;
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

unsigned MIPSXTargetLowering::getJumpTableEncoding() const {
  return MachineJumpTableInfo::EK_Custom32;
}

const MCExpr *MIPSXTargetLowering::LowerCustomJumpTableEntry(
    const MachineJumpTableInfo * /*MJTI*/, const MachineBasicBlock *MBB,
    unsigned /*UID*/, MCContext &Ctx) const {
  return MCSpecifierExpr::create(MCSymbolRefExpr::create(MBB->getSymbol(), Ctx),
                                 MIPSX::S_W, Ctx);
}

SDValue MIPSXTargetLowering::LowerSELECT_CC(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0), RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2), FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  return DAG.getNode(MIPSXISD::SELECT_CC, DL, Op.getValueType(), TrueV, FalseV,
                     LHS, RHS, DAG.getTargetConstant(CC, DL, MVT::i32));
}

SDValue MIPSXTargetLowering::LowerShift(SDValue Op, SelectionDAG &DAG) const {
  // Shifts by a constant have patterns.
  if (isa<ConstantSDNode>(Op.getOperand(1)))
    return Op;

  RTLIB::Libcall LC;
  switch (Op.getOpcode()) {
  case ISD::SHL:
    LC = RTLIB::SHL_I32;
    break;
  case ISD::SRL:
    LC = RTLIB::SRL_I32;
    break;
  default:
    LC = RTLIB::SRA_I32;
    break;
  }
  SDValue Ops[2] = {Op.getOperand(0),
                    DAG.getZExtOrTrunc(Op.getOperand(1), SDLoc(Op), MVT::i32)};
  MakeLibCallOptions CallOptions;
  return makeLibCall(DAG, LC, MVT::i32, Ops, CallOptions, SDLoc(Op)).first;
}

// The branch that is taken when (A CC B) holds; gt, le, ugt and ule swap
// the operands (abi section 5.3).
static unsigned getBranchOpcode(ISD::CondCode CC, bool &Swap) {
  Swap = false;
  switch (CC) {
  case ISD::SETEQ:
    return MIPSX::BEQ;
  case ISD::SETNE:
    return MIPSX::BNE;
  case ISD::SETLT:
    return MIPSX::BLT;
  case ISD::SETGE:
    return MIPSX::BGE;
  case ISD::SETGT:
    Swap = true;
    return MIPSX::BLT;
  case ISD::SETLE:
    Swap = true;
    return MIPSX::BGE;
  case ISD::SETULT:
    return MIPSX::BLO;
  case ISD::SETUGE:
    return MIPSX::BHS;
  case ISD::SETUGT:
    Swap = true;
    return MIPSX::BLO;
  case ISD::SETULE:
    Swap = true;
    return MIPSX::BHS;
  default:
    llvm_unreachable("Unexpected integer condition code");
  }
}

// SELECT becomes
//   ThisMBB:  b<cc> a,b,SinkMBB      (result is True if taken)
//   FalseMBB: (falls through)
//   SinkMBB:  rd = phi [True, ThisMBB], [False, FalseMBB]
MachineBasicBlock *
MIPSXTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                 MachineBasicBlock *BB) const {
  assert(MI.getOpcode() == MIPSX::SELECT && "Unexpected instruction");
  const TargetInstrInfo &TII = *Subtarget.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = BB->getParent();
  const BasicBlock *LLVMBB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();

  MachineBasicBlock *ThisMBB = BB;
  MachineBasicBlock *FalseMBB = MF->CreateMachineBasicBlock(LLVMBB);
  MachineBasicBlock *SinkMBB = MF->CreateMachineBasicBlock(LLVMBB);
  MF->insert(It, FalseMBB);
  MF->insert(It, SinkMBB);

  SinkMBB->splice(SinkMBB->begin(), BB,
                  std::next(MachineBasicBlock::iterator(MI)), BB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(BB);
  BB->addSuccessor(FalseMBB);
  BB->addSuccessor(SinkMBB);
  FalseMBB->addSuccessor(SinkMBB);

  bool Swap;
  unsigned Opc = getBranchOpcode(
      static_cast<ISD::CondCode>(MI.getOperand(5).getImm()), Swap);
  Register A = MI.getOperand(3).getReg(), B = MI.getOperand(4).getReg();
  BuildMI(BB, DL, TII.get(Opc))
      .addReg(Swap ? B : A)
      .addReg(Swap ? A : B)
      .addMBB(SinkMBB);

  BuildMI(*SinkMBB, SinkMBB->begin(), DL, TII.get(MIPSX::PHI),
          MI.getOperand(0).getReg())
      .addReg(MI.getOperand(1).getReg())
      .addMBB(ThisMBB)
      .addReg(MI.getOperand(2).getReg())
      .addMBB(FalseMBB);

  MI.eraseFromParent();
  return SinkMBB;
}

SDValue MIPSXTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

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

  // The variadic words follow the named stack words (abi section 7.2).
  if (IsVarArg)
    MF.getInfo<MIPSXMachineFunctionInfo>()->setVarArgsFrameIndex(
        MFI.CreateFixedObject(4, CCInfo.getStackSize(), /*IsImmutable=*/true));
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

  // Large aggregates are passed by reference to a copy the caller makes
  // (abi section 7.1); Clang does not use byval for MIPS-X.
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

  // Direct calls go through jspci and a linker-selected code base. With
  // -mattr=+long-calls they take the full address instead, like an
  // indirect call (abi section 5.4). Indirect calls have the callee's byte
  // address in a register.
  if (Subtarget.useLongCalls() && (isa<GlobalAddressSDNode>(Callee) ||
                                   isa<ExternalSymbolSDNode>(Callee))) {
    if (isa<GlobalAddressSDNode>(Callee))
      Callee = LowerGlobalAddress(Callee, DAG);
    else
      Callee = LowerExternalSymbol(Callee, DAG);
  } else if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32,
                                        G->getOffset());
  } else if (auto *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), MVT::i32);
  }

  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &[Reg, Val] : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg, Val.getValueType()));
  // The SDK's helper routines clobber fewer registers than a normal call.
  const MIPSXRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = nullptr;
  if (auto *S = dyn_cast<ExternalSymbolSDNode>(Callee))
    Mask = TRI->getHelperCallPreservedMask(S->getSymbol());
  if (!Mask)
    Mask = TRI->getCallPreservedMask(MF, CallConv);
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
