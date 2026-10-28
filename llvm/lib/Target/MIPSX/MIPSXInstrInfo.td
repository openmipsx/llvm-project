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
def simm17 : Operand<i32> {
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
// Memory instructions (TY = 10)
//===----------------------------------------------------------------------===//

def LD : FLoad<0b000, "ld", memoff17>;
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
