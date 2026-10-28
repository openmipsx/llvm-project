//===-- MIPSXDisassembler.cpp - Disassembler for MIPS-X -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Decodes 32-bit big-endian MIPS-X instruction words with the generated
// decoder tables. Words that match no instruction, including those with
// non-zero bits in fields an instruction leaves unused, fail to decode.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "TargetInfo/MIPSXTargetInfo.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"

#define DEBUG_TYPE "mipsx-disassembler"

using namespace llvm;
using namespace llvm::MCD;

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class MIPSXDisassembler : public MCDisassembler {
public:
  MIPSXDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static const MCPhysReg GPRDecoderTable[] = {
    MIPSX::R0,  MIPSX::R1,  MIPSX::R2,  MIPSX::R3,  MIPSX::R4,  MIPSX::R5,
    MIPSX::R6,  MIPSX::R7,  MIPSX::R8,  MIPSX::R9,  MIPSX::R10, MIPSX::R11,
    MIPSX::R12, MIPSX::R13, MIPSX::R14, MIPSX::R15, MIPSX::R16, MIPSX::R17,
    MIPSX::R18, MIPSX::R19, MIPSX::R20, MIPSX::R21, MIPSX::R22, MIPSX::R23,
    MIPSX::R24, MIPSX::R25, MIPSX::R26, MIPSX::R27, MIPSX::R28, MIPSX::R29,
    MIPSX::R30, MIPSX::R31};

static DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, unsigned RegNo,
                                           uint64_t /*Address*/,
                                           const MCDisassembler * /*Decoder*/) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

// Spec field of movfrs: 1 psw, 2 md, 4 PC-4. Other values are undefined.
static DecodeStatus DecodeSPRRdRegisterClass(MCInst &Inst, unsigned Spec,
                                             uint64_t /*Address*/,
                                             const MCDisassembler *) {
  MCRegister Reg;
  switch (Spec) {
  case 1:
    Reg = MIPSX::PSW;
    break;
  case 2:
    Reg = MIPSX::MD;
    break;
  case 4:
    Reg = MIPSX::PCM4;
    break;
  default:
    return MCDisassembler::Fail;
  }
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

// Spec field of movtos: 1 psw, 2 md, 4 PC-1.
static DecodeStatus DecodeSPRWrRegisterClass(MCInst &Inst, unsigned Spec,
                                             uint64_t /*Address*/,
                                             const MCDisassembler *) {
  MCRegister Reg;
  switch (Spec) {
  case 1:
    Reg = MIPSX::PSW;
    break;
  case 2:
    Reg = MIPSX::MD;
    break;
  case 4:
    Reg = MIPSX::PCM1;
    break;
  default:
    return MCDisassembler::Fail;
  }
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmOperand(MCInst &Inst, uint64_t Imm,
                                      uint64_t /*Address*/,
                                      const MCDisassembler *) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm)));
  return MCDisassembler::Success;
}

// 7-bit shift field: bits 6-4 bbb, bits 3-0 one-hot dddd, e = bbb * 4 +
// log2(dddd), n = 32 - e (1 to 32). A dddd that is not one-hot is invalid.
static DecodeStatus decodeShiftAmount(MCInst &Inst, uint64_t Field,
                                      uint64_t /*Address*/,
                                      const MCDisassembler *) {
  unsigned D = Field & 0xf;
  if (!isPowerOf2_32(D))
    return MCDisassembler::Fail;
  unsigned E = ((Field >> 4) & 7) * 4 + Log2_32(D);
  Inst.addOperand(MCOperand::createImm(32 - E));
  return MCDisassembler::Success;
}

// The trap vector is stored as its one's complement.
static DecodeStatus decodeTrapVector(MCInst &Inst, uint64_t Field,
                                     uint64_t /*Address*/,
                                     const MCDisassembler *) {
  Inst.addOperand(MCOperand::createImm(~Field & 0xff));
  return MCDisassembler::Success;
}

#include "MIPSXGenDisassemblerTables.inc"

DecodeStatus MIPSXDisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream & /*CStream*/) const {
  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }
  Size = 4;
  uint32_t Insn = support::endian::read32be(Bytes.data());
  return decodeInstruction(DecoderTableMIPSX32, Instr, Insn, Address, this,
                           STI);
}

static MCDisassembler *createMIPSXDisassembler(const Target & /*T*/,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new MIPSXDisassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMIPSXDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheMIPSXTarget(),
                                         createMIPSXDisassembler);
}
