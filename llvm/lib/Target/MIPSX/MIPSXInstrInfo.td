//===-- MIPSXInstrInfo.td - MIPS-X instructions ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Every instruction of processor specification sections 3 to 6. Assembly
// syntax follows the manual: sources first, destination last, '#' before
// immediates, "offset[base]" for memory operands (MIPS-X SDK docs/abi.md
// section 10).
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Operands
//===----------------------------------------------------------------------===//

// 17-bit signed immediates and memory offsets: a constant, a symbolic
// expression, or a relocation operator. Which operators are allowed depends
// on the instruction (docs/abi.md section 10.6).
class Imm17Operand<string name> : AsmOperandClass {
  let Name = name;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "Invalid" # name;
}

// addi immediate: %hi, %lo, %gprel, %w.
def simm17 : Operand<i32>, ImmLeaf<i32, [{ return isInt<17>(Imm); }]> {
  let ParserMatchClass = Imm17Operand<"Imm17">;
  let EncoderMethod = "getImm17OpValue";
  let DecoderMethod = "decodeSImmOperand<17>";
}

// Memory offset: %hi, %lo, %gprel.
def memoff17 : Operand<i32> {
  let ParserMatchClass = Imm17Operand<"MemOff17">;
  let EncoderMethod = "getImm17OpValue";
  let DecoderMethod = "decodeSImmOperand<17>";
}

// jspci immediate: %call, %w.
def jimm17 : Operand<i32> {
  let ParserMatchClass = Imm17Operand<"JImm17">;
  let EncoderMethod = "getImm17OpValue";
  let DecoderMethod = "decodeSImmOperand<17>";
}

// Branch target: a label, or a constant giving the 16-bit word displacement
// field itself (target word = branch word + 2 + displacement).
def BrTargetAsmOperand : AsmOperandClass {
  let Name = "BrTarget";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidBrTarget";
}
def brtarget : Operand<OtherVT> {
  let ParserMatchClass = BrTargetAsmOperand;
  let EncoderMethod = "getBranchTargetOpValue";
  let DecoderMethod = "decodeSImmOperand<16>";
  let PrintMethod = "printBranchTarget";
  let OperandType = "OPERAND_PCREL";
}

// Right-shift amount n of sh and asr, 1 to 32.
def ShAmtAsmOperand : AsmOperandClass {
  let Name = "ShAmt";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidShAmt";
}
def shamt : Operand<i32> {
  let ParserMatchClass = ShAmtAsmOperand;
  let EncoderMethod = "getShiftAmountOpValue";
  let DecoderMethod = "decodeShiftAmount";
}

// Trap vector, 0 to 255.
def TrapVecAsmOperand : AsmOperandClass {
  let Name = "TrapVec";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidTrapVec";
}
def trapvec : Operand<i32> {
  let ParserMatchClass = TrapVecAsmOperand;
  let EncoderMethod = "getTrapVectorOpValue";
  let DecoderMethod = "decodeTrapVector";
}

include "MIPSXInstrFormats.td"

//===----------------------------------------------------------------------===//
// Selection DAG nodes
//===----------------------------------------------------------------------===//

def SDT_MIPSXCall : SDTypeProfile<0, -1, [SDTCisVT<0, i32>]>;
def SDT_MIPSXCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                            SDTCisVT<1, i32>]>;
def SDT_MIPSXCallSeqEnd : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;

// Direct call through jspci and a linker-selected code base.
def MIPSXcall : SDNode<"MIPSXISD::CALL", SDT_MIPSXCall,
                       [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                        SDNPVariadic]>;
// Return: jspci r31,#0,r0.
def MIPSXret : SDNode<"MIPSXISD::RET_GLUE", SDTNone,
                      [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_MIPSXCallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end : SDNode<"ISD::CALLSEQ_END", SDT_MIPSXCallSeqEnd,
                         [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def SDT_MIPSXSelectCC
    : SDTypeProfile<1, 5, [SDTCisVT<0, i32>, SDTCisSameAs<0, 1>,
                           SDTCisSameAs<1, 2>, SDTCisSameAs<3, 4>,
                           SDTCisVT<5, i32>]>;
// (select_cc true, false, lhs, rhs, condcode): lowered to a branch.
def MIPSXselectcc : SDNode<"MIPSXISD::SELECT_CC", SDT_MIPSXSelectCC>;

// Jump to a word address (a jump-table entry, abi section 5.6).
def MIPSXbrindw : SDNode<"MIPSXISD::BRIND_W", SDTBrind,
                         [SDNPHasChain]>;

// Address operands of loads and stores: (base register or frame index,
// 17-bit offset).
def AddrRegImm : ComplexPattern<iPTR, 2, "selectAddrRegImm", [frameindex], []>;

//===----------------------------------------------------------------------===//
// Memory instructions (TY = 10)
//===----------------------------------------------------------------------===//

def LD : FLoad<0b000, "ld", memoff17>;
// Spill reloads (loadRegFromStackSlot): ld without FLoad's early-clobber,
// which the spiller does not model (it gives reloads a normal def slot, so
// an early-clobber reload corrupts live intervals). Frame accesses use sp,
// fp or r1 as the base, which are reserved, so P2 holds anyway.
let isCodeGenOnly = 1, hasSideEffects = 0, Constraints = "" in
def LDfi : FLoad<0b000, "ld", memoff17>;
def ST : FStore<0b010, "st", memoff17>;

// The ES3890 reuses the manual's coprocessor and floating-point memory
// opcodes as byte and halfword accesses (processor spec section 3).
let Predicates = [HasES3890] in {
  def LDBU : FLoad<0b100, "ldbu", memoff17>;
  def LDB  : FLoad<0b101, "ldb", memoff17>;
  def LDHU : FLoad<0b001, "ldhu", memoff17>;
  def LDH  : FLoad<0b111, "ldh", memoff17>;
  def STB  : FStore<0b011, "stb", memoff17>;
  def STH  : FStore<0b110, "sth", memoff17>;
}

//===----------------------------------------------------------------------===//
// Branches (TY = 00)
//===----------------------------------------------------------------------===//

multiclass Branch<bits<3> cond, string opstr> {
  def NAME : FBranch<cond, 0, opstr>;
  def SQ : FBranch<cond, 1, opstr # "sq">;
}

defm BEQ : Branch<0b001, "beq">;
defm BNE : Branch<0b101, "bne">;
defm BHS : Branch<0b010, "bhs">;
defm BLO : Branch<0b110, "blo">;
defm BLT : Branch<0b011, "blt">;
defm BGE : Branch<0b111, "bge">;

//===----------------------------------------------------------------------===//
// Compute instructions (TY = 01)
//===----------------------------------------------------------------------===//

class ALU3<bits<3> op, bits<12> func, string opstr>
    : FCompute<op, func, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
               opstr # "\t$rs1,$rs2,$rd">;

def ADD   : ALU3<0b100, 0x019, "add">;
def SUB   : ALU3<0b100, 0x066, "sub">;
def SUBNC : ALU3<0b100, 0x026, "subnc">;
def AND   : ALU3<0b100, 0x023, "and">;
def BIC   : ALU3<0b100, 0x00B, "bic">;
def OR    : ALU3<0b100, 0x03B, "or">;
def XOR   : ALU3<0b100, 0x01B, "xor">;

let rs2 = 0 in
def NOT : FCompute<0b100, 0x00F, (outs GPR:$rd), (ins GPR:$rs1),
                   "not\t$rs1,$rd">;

def ROTLB  : ALU3<0b001, 0x0C0, "rotlb">;
def ROTLCB : ALU3<0b001, 0x080, "rotlcb">;

// Multiply and divide steps use and update MD.
let Uses = [MD], Defs = [MD] in {
  let rs1 = 0 in
  def MSTART : FCompute<0b000, 0x0E6, (outs GPR:$rd), (ins GPR:$rs2),
                        "mstart\t$rs2,$rd">;
  def MSTEP : ALU3<0b000, 0x099, "mstep">;
  def DSTEP : ALU3<0b000, 0x166, "dstep">;
}

// Funnel shift: Dest = low 32 bits of ((Src2 : Src1) >> n).
def SH : FShift<0b00100, (ins GPR:$rs1, GPR:$rs2, shamt:$amt),
                "sh\t$rs1,$rs2,$rd,#$amt">;
// Arithmetic shift right: Dest = Src1 >> n.
let rs2 = 0 in
def ASR : FShift<0b00010, (ins GPR:$rs1, shamt:$amt),
                 "asr\t$rs1,$rd,#$amt">;

//===----------------------------------------------------------------------===//
// Compute immediate instructions (TY = 11)
//===----------------------------------------------------------------------===//

def ADDI : FImm<0b100, (outs GPR:$rd), (ins GPR:$rs1, simm17:$imm),
                "addi\t$rs1,#$imm,$rd">;

// Jump to word address Src1 + imm; Dest = this word address + 3. Both delay
// slots always execute.
let hasDelaySlot = 1, hasSideEffects = 1 in
def JSPCI : FImm<0b000, (outs GPR:$rd), (ins GPR:$rs1, jimm17:$imm),
                 "jspci\t$rs1,#$imm,$rd">;

let hasSideEffects = 1 in {
  def MOVFRS : MIPSXInst<0b11, 0b011, (outs GPR:$rd), (ins SPRRd:$spr),
                         "movfrs\t$spr,$rd"> {
    bits<5> rd;
    bits<3> spr;
    let Inst{26-17} = 0;
    let Inst{16-12} = rd;
    let Inst{11-3} = 0;
    let Inst{2-0} = spr;
  }

  def MOVTOS : MIPSXInst<0b11, 0b010, (outs SPRWr:$spr), (ins GPR:$rs1),
                         "movtos\t$rs1,$spr"> {
    bits<5> rs1;
    bits<3> spr;
    let Inst{26-22} = rs1;
    let Inst{21-3} = 0;
    let Inst{2-0} = spr;
  }
}

let hasDelaySlot = 1, hasSideEffects = 1, isBarrier = 1,
    isTerminator = 1 in {
  def JPC   : FImmFunc<0b101, 0x003, "jpc">;
  def JPCRS : FImmFunc<0b111, 0x003, "jpcrs">;

  // Jump to word vector * 8. Func bits 10-3 hold the one's complement of the
  // vector.
  def TRAP : MIPSXInst<0b11, 0b110, (outs), (ins trapvec:$vec),
                       "trap\t#$vec"> {
    bits<8> vec;
    let Inst{26-12} = 0;
    let Inst{11} = 0;
    let Inst{10-3} = vec;
    let Inst{2-0} = 0b011;
  }
}

// Halt (word 0xCFC00000: Src1 = r31, everything else 0).
let hasSideEffects = 1, isBarrier = 1, isTerminator = 1 in
def HSC : MIPSXInst<0b11, 0b001, (outs), (ins), "hsc"> {
  let Inst{26-22} = 0b11111;
  let Inst{21-0} = 0;
}

//===----------------------------------------------------------------------===//
// Aliases (docs/abi.md section 10.5). The others (li, la, call, jmp, lsl,
// lsr and the swapped branch conditions) are expanded by the assembly
// parser; lsl and lsr are printed by MIPSXInstPrinter.
//===----------------------------------------------------------------------===//

def : InstAlias<"nop", (ADD R0, R0, R0), 2>;
def : InstAlias<"mov\t$rs1,$rd", (ADD GPR:$rd, GPR:$rs1, R0), 1>;
def : InstAlias<"ret", (JSPCI R0, R31, 0), 1>;
def : InstAlias<"b\t$dst", (BEQ R0, R0, brtarget:$dst), 1>;
def : InstAlias<"bra\t$dst", (BEQ R0, R0, brtarget:$dst), 0>;
def : InstAlias<"bsq\t$dst", (BEQSQ R0, R0, brtarget:$dst), 1>;

//===----------------------------------------------------------------------===//
// Code generation pseudo-instructions
//===----------------------------------------------------------------------===//

// The call frame is reserved in the prologue, so these only mark the call
// sequence.
let Defs = [R29], Uses = [R29] in {
  def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                [(callseq_start timm:$amt1, timm:$amt2)]>;
  def ADJCALLSTACKUP : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                              [(callseq_end timm:$amt1, timm:$amt2)]>;
}

// A direct call, emitted as jspci r0,#%call(sym),r31. The delay slots are
// added by MIPSXHazardPadding.
def calltarget : Operand<i32>;
let isCall = 1, hasDelaySlot = 1, Defs = [R31], Uses = [R29] in
def CALL : Pseudo<(outs), (ins calltarget:$dst), []>;
def : Pat<(MIPSXcall tglobaladdr:$dst), (CALL tglobaladdr:$dst)>;
def : Pat<(MIPSXcall texternalsym:$dst), (CALL texternalsym:$dst)>;

// An indirect call to the word address in rs, emitted as jspci rs,#0,r31.
// Function pointers are byte addresses, so the selected code shifts right
// by 2 first (abi section 5.6).
let isCall = 1, hasDelaySlot = 1, Defs = [R31], Uses = [R29] in
def CALLR : Pseudo<(outs), (ins GPR:$rs), []>;
def : Pat<(MIPSXcall GPR:$fn), (CALLR (SH GPR:$fn, R0, 2))>;

// Return, emitted as jspci r31,#0,r0. r31 is not an implicit use: it is in
// the callee-saved list, so if register allocation uses it the prologue saves
// and the epilogue restores it (as RISC-V does with ra).
let isReturn = 1, isTerminator = 1, isBarrier = 1, hasDelaySlot = 1 in
def RET : Pseudo<(outs), (ins), [(MIPSXret)]>;

// Unconditional branch: beq r0,r0,label, marked as a barrier.
let isBranch = 1, isTerminator = 1, isBarrier = 1, hasDelaySlot = 1,
    isCodeGenOnly = 1 in
def B : MIPSXInst<0b00, 0b001, (outs), (ins brtarget:$dst), "b\t$dst"> {
  bits<16> dst;
  let Inst{26-16} = 0;
  let Inst{15-0} = dst;
}
def : Pat<(br bb:$dst), (B bb:$dst)>;

// Indirect branch to the word address in rs: jspci rs,#0,r0. Jump-table
// entries are word addresses (docs/abi.md section 5.6); other targets, such
// as block addresses, are byte addresses and are shifted right by 2 first
// (abi section 4).
let isBranch = 1, isIndirectBranch = 1, isTerminator = 1, isBarrier = 1,
    hasDelaySlot = 1, isCodeGenOnly = 1 in
def BRIND : MIPSXInst<0b11, 0b000, (outs), (ins GPR:$rs1), "jspci\t$rs1,#0,r0",
                      [(MIPSXbrindw GPR:$rs1)]> {
  bits<5> rs1;
  let Inst{26-22} = rs1;
  let Inst{21-0} = 0;
}

// Far jump: jspci r0,#%call(label),r0, for branches out of range of a
// branch displacement (abi section 5.5). Inserted by branch relaxation.
def jumptarget : Operand<OtherVT> {
  let EncoderMethod = "getImm17OpValue";
}
let isBranch = 1, isTerminator = 1, isBarrier = 1, hasDelaySlot = 1,
    isCodeGenOnly = 1 in
def JMP : MIPSXInst<0b11, 0b000, (outs), (ins jumptarget:$dst),
                    "jspci\tr0,#$dst,r0"> {
  bits<17> dst;
  let Inst{26-17} = 0;
  let Inst{16-0} = dst;
}

// select_cc, expanded into a branch diamond by EmitInstrWithCustomInserter.
let usesCustomInserter = 1 in
def SELECT : Pseudo<(outs GPR:$rd),
                    (ins GPR:$t, GPR:$f, GPR:$a, GPR:$b, i32imm:$cc),
                    [(set GPR:$rd, (MIPSXselectcc GPR:$t, GPR:$f, GPR:$a,
                                                  GPR:$b, timm:$cc))]>;

//===----------------------------------------------------------------------===//
// Patterns
//===----------------------------------------------------------------------===//

def : Pat<(brind GPR:$rs1), (BRIND (SH GPR:$rs1, R0, 2))>;

// Conditional branches. gt, le, ugt and ule swap the operands (abi section
// 5.3).
def : Pat<(brcc SETEQ, GPR:$a, GPR:$b, bb:$d), (BEQ GPR:$a, GPR:$b, bb:$d)>;
def : Pat<(brcc SETNE, GPR:$a, GPR:$b, bb:$d), (BNE GPR:$a, GPR:$b, bb:$d)>;
def : Pat<(brcc SETLT, GPR:$a, GPR:$b, bb:$d), (BLT GPR:$a, GPR:$b, bb:$d)>;
def : Pat<(brcc SETGE, GPR:$a, GPR:$b, bb:$d), (BGE GPR:$a, GPR:$b, bb:$d)>;
def : Pat<(brcc SETGT, GPR:$a, GPR:$b, bb:$d), (BLT GPR:$b, GPR:$a, bb:$d)>;
def : Pat<(brcc SETLE, GPR:$a, GPR:$b, bb:$d), (BGE GPR:$b, GPR:$a, bb:$d)>;
def : Pat<(brcc SETULT, GPR:$a, GPR:$b, bb:$d), (BLO GPR:$a, GPR:$b, bb:$d)>;
def : Pat<(brcc SETUGE, GPR:$a, GPR:$b, bb:$d), (BHS GPR:$a, GPR:$b, bb:$d)>;
def : Pat<(brcc SETUGT, GPR:$a, GPR:$b, bb:$d), (BLO GPR:$b, GPR:$a, bb:$d)>;
def : Pat<(brcc SETULE, GPR:$a, GPR:$b, bb:$d), (BHS GPR:$b, GPR:$a, bb:$d)>;

// Constants (docs/abi.md section 6.1): one addi if they fit in 17 bits,
// two instructions for a 17-bit value shifted left, otherwise the hi/lo
// form.
def ShiftedImm17 : ImmLeaf<i32, [{
  uint32_t V = Imm;
  if (isInt<17>(Imm) || V == 0)
    return false;
  unsigned T = llvm::countr_zero(V);
  return T > 0 && isInt<17>(int32_t(V) >> T);
}]>;
def ShiftedVal : SDNodeXForm<imm, [{
  uint32_t V = N->getZExtValue();
  return CurDAG->getSignedTargetConstant(int32_t(V) >> llvm::countr_zero(V),
                                         SDLoc(N), MVT::i32);
}]>;
def ShiftedAmt : SDNodeXForm<imm, [{
  uint32_t V = N->getZExtValue();
  return CurDAG->getTargetConstant(32 - llvm::countr_zero(V), SDLoc(N),
                                   MVT::i32);
}]>;
def HI17 : SDNodeXForm<imm, [{
  return CurDAG->getSignedTargetConstant(int32_t(N->getSExtValue()) >> 15,
                                         SDLoc(N), MVT::i32);
}]>;
def LO15 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getZExtValue() & 0x7fff, SDLoc(N),
                                   MVT::i32);
}]>;
def : Pat<(i32 simm17:$imm), (ADDI R0, simm17:$imm)>;
let AddedComplexity = 1 in
def : Pat<(i32 ShiftedImm17:$imm),
          (SH R0, (ADDI R0, (ShiftedVal imm:$imm)), (ShiftedAmt imm:$imm))>;
def : Pat<(i32 imm:$imm),
          (ADDI (SH R0, (ADDI R0, (HI17 imm:$imm)), 17), (LO15 imm:$imm))>;

// Arithmetic and logic.
def : Pat<(add GPR:$a, GPR:$b), (ADD GPR:$a, GPR:$b)>;
def : Pat<(add GPR:$a, simm17:$imm), (ADDI GPR:$a, simm17:$imm)>;
def : Pat<(sub GPR:$a, GPR:$b), (SUB GPR:$a, GPR:$b)>;
def : Pat<(and GPR:$a, GPR:$b), (AND GPR:$a, GPR:$b)>;
def : Pat<(or GPR:$a, GPR:$b), (OR GPR:$a, GPR:$b)>;
def : Pat<(xor GPR:$a, GPR:$b), (XOR GPR:$a, GPR:$b)>;
def : Pat<(not GPR:$a), (NOT GPR:$a)>;
def : Pat<(and GPR:$b, (not GPR:$a)), (BIC GPR:$a, GPR:$b)>;

// Shifts by a constant: lsl is sh r0,rs,rd,#(32-k), lsr is sh rs,r0,rd,#k.
def uimm5nz : ImmLeaf<i32, [{ return Imm > 0 && Imm < 32; }]>;
def ShlAmt : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(32 - N->getZExtValue(), SDLoc(N), MVT::i32);
}]>;
def : Pat<(shl GPR:$a, uimm5nz:$k), (SH R0, GPR:$a, (ShlAmt imm:$k))>;
def : Pat<(srl GPR:$a, uimm5nz:$k), (SH GPR:$a, R0, uimm5nz:$k)>;
def : Pat<(sra GPR:$a, uimm5nz:$k), (ASR GPR:$a, uimm5nz:$k)>;

// Loads and stores of every width.
class LoadPat<PatFrag op, Instruction inst>
    : Pat<(i32 (op (AddrRegImm GPR:$rs1, memoff17:$off))),
          (inst GPR:$rs1, memoff17:$off)>;
class StorePat<PatFrag op, Instruction inst>
    : Pat<(op GPR:$val, (AddrRegImm GPR:$rs1, memoff17:$off)),
          (inst GPR:$val, GPR:$rs1, memoff17:$off)>;

def : LoadPat<load, LD>;
def : StorePat<store, ST>;
let Predicates = [HasES3890] in {
  def : LoadPat<sextloadi8, LDB>;
  def : LoadPat<zextloadi8, LDBU>;
  def : LoadPat<extloadi8, LDBU>;
  def : LoadPat<sextloadi16, LDH>;
  def : LoadPat<zextloadi16, LDHU>;
  def : LoadPat<extloadi16, LDHU>;
  def : StorePat<truncstorei8, STB>;
  def : StorePat<truncstorei16, STH>;
}
