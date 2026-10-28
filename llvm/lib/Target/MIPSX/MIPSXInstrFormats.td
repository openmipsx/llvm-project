//===-- MIPSXInstrFormats.td - MIPS-X instruction formats --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Bits are numbered from the least significant bit, as in the processor
// specification (the manual numbers them the other way round).
//
//   31-30 TY    00 branch, 01 compute, 10 memory, 11 compute immediate
//   29-27 OP    (COND for branches)
//   26-22 Src1
//   21-17 Src2  (Dest for loads, addi and jspci)
//   16-12 Dest  (compute instructions and movfrs)
//   11-0  Comp Func
//   16-0  Offset / Immed, 17-bit signed
//   16    SQ (branches), 15-0 Disp (branches, in words)
//
//===----------------------------------------------------------------------===//

class MIPSXInst<bits<2> ty, bits<3> op, dag outs, dag ins, string asmstr,
                list<dag> pattern = []>
    : Instruction {
  field bits<32> Inst;
  field bits<32> SoftFail = 0;
  let Size = 4;
  let Namespace = "MIPSX";
  let DecoderNamespace = "MIPSX";

  let Inst{31-30} = ty;
  let Inst{29-27} = op;

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

// Memory: address = Src1 + offset (bytes). Loads write the register in the
// Src2 field.
class FLoad<bits<3> op, string opstr, Operand offty>
    : MIPSXInst<0b10, op, (outs GPR:$rd), (ins GPR:$rs1, offty:$off),
                opstr # "\t$off[$rs1],$rd"> {
  bits<5> rs1;
  bits<5> rd;
  bits<17> off;
  let Inst{26-22} = rs1;
  let Inst{21-17} = rd;
  let Inst{16-0} = off;
  let mayLoad = 1;
  // A load must not overwrite its own address register (abi section 9, P2).
  let Constraints = "@earlyclobber $rd";
}

class FStore<bits<3> op, string opstr, Operand offty>
    : MIPSXInst<0b10, op, (outs), (ins GPR:$rs2, GPR:$rs1, offty:$off),
                opstr # "\t$off[$rs1],$rs2"> {
  bits<5> rs1;
  bits<5> rs2;
  bits<17> off;
  let Inst{26-22} = rs1;
  let Inst{21-17} = rs2;
  let Inst{16-0} = off;
  let mayStore = 1;
}

// Branch on Src1 - Src2. sq = 1 squashes both delay slots when not taken.
class FBranch<bits<3> cond, bit sq, string opstr>
    : MIPSXInst<0b00, cond, (outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$dst),
                opstr # "\t$rs1,$rs2,$dst"> {
  bits<5> rs1;
  bits<5> rs2;
  bits<16> dst;
  let Inst{26-22} = rs1;
  let Inst{21-17} = rs2;
  let Inst{16} = sq;
  let Inst{15-0} = dst;
  let isBranch = 1;
  let isTerminator = 1;
  let hasDelaySlot = 1;
}

// Compute: Dest = Src1 op Src2. Unused register fields are set to 0 by the
// instruction definitions.
class FCompute<bits<3> op, bits<12> func, dag outs, dag ins, string asmstr>
    : MIPSXInst<0b01, op, outs, ins, asmstr> {
  bits<5> rs1;
  bits<5> rs2;
  bits<5> rd;
  let Inst{26-22} = rs1;
  let Inst{21-17} = rs2;
  let Inst{16-12} = rd;
  let Inst{11-0} = func;
}

// Shifts: Func bits 11-7 select sh or asr, bits 6-0 hold the encoded amount.
class FShift<bits<5> kind, dag ins, string asmstr>
    : MIPSXInst<0b01, 0b001, (outs GPR:$rd), ins, asmstr> {
  bits<5> rs1;
  bits<5> rs2;
  bits<5> rd;
  bits<7> amt;
  let Inst{26-22} = rs1;
  let Inst{21-17} = rs2;
  let Inst{16-12} = rd;
  let Inst{11-7} = kind;
  let Inst{6-0} = amt;
}

// Compute immediate with a 17-bit immediate and Dest in the Src2 field.
class FImm<bits<3> op, dag outs, dag ins, string asmstr>
    : MIPSXInst<0b11, op, outs, ins, asmstr> {
  bits<5> rs1;
  bits<5> rd;
  bits<17> imm;
  let Inst{26-22} = rs1;
  let Inst{21-17} = rd;
  let Inst{16-0} = imm;
}

// Compute immediate with a fixed Comp Func field and no operands (jpc, jpcrs).
class FImmFunc<bits<3> op, bits<12> func, string asmstr>
    : MIPSXInst<0b11, op, (outs), (ins), asmstr> {
  let Inst{26-12} = 0;
  let Inst{11-0} = func;
}

// Code generation pseudo-instructions, expanded before or during emission.
class Pseudo<dag outs, dag ins, list<dag> pattern>
    : MIPSXInst<0b00, 0b000, outs, ins, "", pattern> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Inst = 0;
}
