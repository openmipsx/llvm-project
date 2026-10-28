//===-- MIPSXAsmPrinter.cpp - MIPS-X LLVM assembly writer -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Lowers MachineInstrs to MCInsts and emits them. The CALL and RET pseudos
// become jspci (docs/abi.md section 5.1); a branch or jump is emitted with
// the delay-slot instructions bundled after it.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXInstPrinter.h"
#include "MCTargetDesc/MIPSXMCAsmInfo.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "MIPSX.h"
#include "MIPSXInstrInfo.h"
#include "TargetInfo/MIPSXTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {

class MIPSXAsmPrinter : public AsmPrinter {
public:
  static char ID;

  explicit MIPSXAsmPrinter(TargetMachine &TM,
                           std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "MIPS-X Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;

private:
  MCOperand lowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym,
                               MIPSX::Specifier Spec) const;
  bool lowerOperand(const MachineOperand &MO, MCOperand &MCOp) const;
  void lowerInstruction(const MachineInstr *MI, MCInst &OutMI) const;
};

} // end anonymous namespace

MCOperand MIPSXAsmPrinter::lowerSymbolOperand(const MachineOperand &MO,
                                              MCSymbol *Sym,
                                              MIPSX::Specifier Spec) const {
  const MCExpr *Expr = MCSymbolRefExpr::create(Sym, OutContext);
  if (!MO.isJTI() && !MO.isMBB() && MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), OutContext), OutContext);
  if (Spec != MIPSX::S_None)
    Expr = MCSpecifierExpr::create(Expr, Spec, OutContext);
  return MCOperand::createExpr(Expr);
}

static MIPSX::Specifier getSpecifier(const MachineOperand &MO) {
  switch (MO.getTargetFlags()) {
  case MIPSXII::MO_NO_FLAG:
    return MIPSX::S_None;
  case MIPSXII::MO_HI:
    return MIPSX::S_HI;
  case MIPSXII::MO_LO:
    return MIPSX::S_LO;
  }
  llvm_unreachable("Unknown target flag on a symbol operand");
}

bool MIPSXAsmPrinter::lowerOperand(const MachineOperand &MO,
                                   MCOperand &MCOp) const {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      return false;
    MCOp = MCOperand::createReg(MO.getReg());
    return true;
  case MachineOperand::MO_Immediate:
    MCOp = MCOperand::createImm(MO.getImm());
    return true;
  case MachineOperand::MO_MachineBasicBlock:
    MCOp = lowerSymbolOperand(MO, MO.getMBB()->getSymbol(), MIPSX::S_None);
    return true;
  case MachineOperand::MO_GlobalAddress:
    MCOp = lowerSymbolOperand(MO, getSymbol(MO.getGlobal()), getSpecifier(MO));
    return true;
  case MachineOperand::MO_ExternalSymbol:
    MCOp = lowerSymbolOperand(
        MO, GetExternalSymbolSymbol(MO.getSymbolName()), getSpecifier(MO));
    return true;
  case MachineOperand::MO_RegisterMask:
    return false;
  default:
    report_fatal_error("MIPS-X: unsupported machine operand");
  }
}

void MIPSXAsmPrinter::lowerInstruction(const MachineInstr *MI,
                                       MCInst &OutMI) const {
  switch (MI->getOpcode()) {
  case MIPSX::CALL: {
    // jspci r0,#%call(sym),r31; the linker picks the code base.
    const MachineOperand &Target = MI->getOperand(0);
    MCSymbol *Sym = Target.isGlobal()
                        ? getSymbol(Target.getGlobal())
                        : GetExternalSymbolSymbol(Target.getSymbolName());
    OutMI = MCInstBuilder(MIPSX::JSPCI)
                .addReg(MIPSX::R31)
                .addReg(MIPSX::R0)
                .addOperand(lowerSymbolOperand(Target, Sym, MIPSX::S_CALL));
    return;
  }
  case MIPSX::RET:
    // jspci r31,#0,r0
    OutMI = MCInstBuilder(MIPSX::JSPCI)
                .addReg(MIPSX::R0)
                .addReg(MIPSX::R31)
                .addImm(0);
    return;
  default:
    break;
  }

  OutMI.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp;
    if (lowerOperand(MO, MCOp))
      OutMI.addOperand(MCOp);
  }
}

void MIPSXAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MIPSX_MC::verifyInstructionPredicates(MI->getOpcode(),
                                        getSubtargetInfo().getFeatureBits());

  MachineBasicBlock::const_instr_iterator I = MI->getIterator();
  MachineBasicBlock::const_instr_iterator E = MI->getParent()->instr_end();
  do {
    MCInst TmpInst;
    lowerInstruction(&*I, TmpInst);
    EmitToStreamer(*OutStreamer, TmpInst);
  } while (++I != E && I->isInsideBundle());
}

char MIPSXAsmPrinter::ID = 0;

INITIALIZE_PASS(MIPSXAsmPrinter, "mipsx-asm-printer", "MIPS-X Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMIPSXAsmPrinter() {
  RegisterAsmPrinter<MIPSXAsmPrinter> X(getTheMIPSXTarget());
}
