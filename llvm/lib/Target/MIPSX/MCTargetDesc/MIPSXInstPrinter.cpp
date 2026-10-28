//===-- MIPSXInstPrinter.cpp - Convert MIPS-X MCInst to asm syntax --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXInstPrinter.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

#define PRINT_ALIAS_INSTR
#include "MIPSXGenAsmWriter.inc"

void MIPSXInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

void MIPSXInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &STI,
                                 raw_ostream &O) {
  if (!printShiftAlias(MI, O) && !printAliasInstr(MI, Address, O))
    printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

// sh r0,rs,rd,#n is "lsl rs,rd,#(32-n)" (for any rs, including r0) and
// sh rs,r0,rd,#n with rs != r0 is "lsr rs,rd,#n" (docs/abi.md section 10.5).
bool MIPSXInstPrinter::printShiftAlias(const MCInst *MI, raw_ostream &O) {
  if (MI->getOpcode() != MIPSX::SH || !MI->getOperand(3).isImm())
    return false;
  MCRegister Rd = MI->getOperand(0).getReg();
  MCRegister Rs1 = MI->getOperand(1).getReg();
  MCRegister Rs2 = MI->getOperand(2).getReg();
  int64_t N = MI->getOperand(3).getImm();
  if (Rs1 == MIPSX::R0) {
    O << "\tlsl\t";
    printRegName(O, Rs2);
    O << ',';
    printRegName(O, Rd);
    O << ",#" << (32 - N);
    return true;
  }
  if (Rs2 == MIPSX::R0) {
    O << "\tlsr\t";
    printRegName(O, Rs1);
    O << ',';
    printRegName(O, Rd);
    O << ",#" << N;
    return true;
  }
  return false;
}

void MIPSXInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                    raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg())
    printRegName(O, Op.getReg());
  else if (Op.isImm())
    O << Op.getImm();
  else {
    assert(Op.isExpr() && "Unknown operand kind in printOperand");
    MAI.printExpr(O, *Op.getExpr());
  }
}

// With PrintBranchImmAsAddress (llvm-objdump), a constant branch target is
// shown as the target byte address: branch + 4 * (displacement + 2).
void MIPSXInstPrinter::printBranchTarget(const MCInst *MI, uint64_t Address,
                                         unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (!Op.isImm() || !PrintBranchImmAsAddress) {
    printOperand(MI, OpNo, O);
    return;
  }
  uint64_t Target = Address + 4 * (Op.getImm() + 2);
  O << formatHex(static_cast<int64_t>(static_cast<uint32_t>(Target)));
}
