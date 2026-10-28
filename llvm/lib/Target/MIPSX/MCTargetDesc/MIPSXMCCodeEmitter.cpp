//===-- MIPSXMCCodeEmitter.cpp - Convert MIPS-X code to machine code ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXFixupKinds.h"
#include "MCTargetDesc/MIPSXMCAsmInfo.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/EndianStream.h"
#include <cassert>
#include <cstdint>

#define DEBUG_TYPE "mccodeemitter"

STATISTIC(MCNumEmitted, "Number of MC instructions emitted");

namespace llvm {

namespace {

class MIPSXMCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;

public:
  MIPSXMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx) : Ctx(Ctx) {}
  MIPSXMCCodeEmitter(const MIPSXMCCodeEmitter &) = delete;
  void operator=(const MIPSXMCCodeEmitter &) = delete;
  ~MIPSXMCCodeEmitter() override = default;

  // TableGen'erated function for getting the binary encoding for an
  // instruction.
  uint64_t getBinaryCodeForInstr(const MCInst &Inst,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  // Return the binary encoding of an operand. If the operand needs a
  // relocation, record a fixup and return zero.
  unsigned getMachineOpValue(const MCInst &Inst, const MCOperand &MCOp,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getImm17OpValue(const MCInst &Inst, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const;

  unsigned getBranchTargetOpValue(const MCInst &Inst, unsigned OpNo,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;

  unsigned getShiftAmountOpValue(const MCInst &Inst, unsigned OpNo,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  unsigned getTrapVectorOpValue(const MCInst &Inst, unsigned OpNo,
                                SmallVectorImpl<MCFixup> &Fixups,
                                const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &Inst, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;
};

} // end anonymous namespace

unsigned MIPSXMCCodeEmitter::getMachineOpValue(
    const MCInst &Inst, const MCOperand &MCOp, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  if (MCOp.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MCOp.getReg());
  if (MCOp.isImm())
    return static_cast<unsigned>(MCOp.getImm());
  llvm_unreachable("Expression operand without an encoder method");
}

// A 17-bit immediate or memory offset. A relocation operator selects the
// fixup kind; a plain symbolic expression is R_MIPSX_ABS17.
static MIPSX::Fixups getImm17FixupKind(const MCExpr *Expr) {
  const auto *SE = dyn_cast<MCSpecifierExpr>(Expr);
  if (!SE)
    return MIPSX::fixup_mipsx_abs17;
  switch (SE->getSpecifier()) {
  case MIPSX::S_HI:
    return MIPSX::fixup_mipsx_hi17;
  case MIPSX::S_LO:
    return MIPSX::fixup_mipsx_lo15;
  case MIPSX::S_GPREL:
    return MIPSX::fixup_mipsx_gprel17;
  case MIPSX::S_CALL:
    return MIPSX::fixup_mipsx_call17;
  case MIPSX::S_W:
    return MIPSX::fixup_mipsx_word17;
  }
  llvm_unreachable("Unknown MIPS-X specifier");
}

unsigned
MIPSXMCCodeEmitter::getImm17OpValue(const MCInst &Inst, unsigned OpNo,
                                    SmallVectorImpl<MCFixup> &Fixups,
                                    const MCSubtargetInfo &STI) const {
  const MCOperand &MCOp = Inst.getOperand(OpNo);
  if (MCOp.isImm())
    return static_cast<unsigned>(MCOp.getImm()) & 0x1ffff;

  assert(MCOp.isExpr() && "Expected an immediate or an expression");
  const MCExpr *Expr = MCOp.getExpr();
  int64_t Value;
  if (Expr->evaluateAsAbsolute(Value))
    return static_cast<unsigned>(Value) & 0x1ffff;
  Fixups.push_back(MCFixup::create(0, Expr, getImm17FixupKind(Expr)));
  return 0;
}

// A constant branch target is the displacement field itself, in words.
unsigned MIPSXMCCodeEmitter::getBranchTargetOpValue(
    const MCInst &Inst, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  const MCOperand &MCOp = Inst.getOperand(OpNo);
  if (MCOp.isImm())
    return static_cast<unsigned>(MCOp.getImm()) & 0xffff;

  assert(MCOp.isExpr() && "Expected an immediate or an expression");
  Fixups.push_back(MCFixup::create(0, MCOp.getExpr(), MIPSX::fixup_mipsx_pc16,
                                   /*PCRel=*/true));
  return 0;
}

unsigned MIPSXMCCodeEmitter::getShiftAmountOpValue(
    const MCInst &Inst, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  int64_t N = Inst.getOperand(OpNo).getImm();
  assert(N >= 1 && N <= 32 && "Shift amount out of range");
  return MIPSX::encodeShiftAmount(N);
}

// The vector is stored as its one's complement.
unsigned
MIPSXMCCodeEmitter::getTrapVectorOpValue(const MCInst &Inst, unsigned OpNo,
                                         SmallVectorImpl<MCFixup> &Fixups,
                                         const MCSubtargetInfo &STI) const {
  int64_t V = Inst.getOperand(OpNo).getImm();
  assert(V >= 0 && V <= 255 && "Trap vector out of range");
  return ~static_cast<unsigned>(V) & 0xff;
}

void MIPSXMCCodeEmitter::encodeInstruction(const MCInst &Inst,
                                           SmallVectorImpl<char> &CB,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  uint32_t Value = getBinaryCodeForInstr(Inst, Fixups, STI);
  ++MCNumEmitted;
  support::endian::write<uint32_t>(CB, Value, llvm::endianness::big);
}

#include "MIPSXGenMCCodeEmitter.inc"

} // end namespace llvm

llvm::MCCodeEmitter *llvm::createMIPSXMCCodeEmitter(const MCInstrInfo &MCII,
                                              MCContext &Ctx) {
  return new MIPSXMCCodeEmitter(MCII, Ctx);
}
