//===-- MIPSXMCTargetDesc.cpp - MIPS-X target descriptions ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXMCTargetDesc.h"
#include "MIPSXInstPrinter.h"
#include "MIPSXMCAsmInfo.h"
#include "TargetInfo/MIPSXTargetInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "MIPSXGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "MIPSXGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "MIPSXGenRegisterInfo.inc"

using namespace llvm;

static MCInstrInfo *createMIPSXMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitMIPSXMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createMIPSXMCRegisterInfo(const Triple & /*TT*/) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitMIPSXMCRegisterInfo(X, MIPSX::R31);
  return X;
}

MCCFIInstruction
MIPSX::createReturnAddressCFI(MCSymbol *L, std::optional<int64_t> CFAOffset) {
  // The expression starts with the CFA on the stack.
  SmallString<16> Expr;
  raw_svector_ostream OS(Expr);
  std::string Comment;
  if (!CFAOffset) {
    OS << uint8_t(dwarf::DW_OP_drop) << uint8_t(dwarf::DW_OP_breg31);
    encodeSLEB128(0, OS);
    Comment = "r31 = r31 * 4";
  } else {
    assert(*CFAOffset < 0 && "saved return address above the CFA");
    uint64_t Below = -*CFAOffset;
    if (Below < 32) {
      OS << uint8_t(dwarf::DW_OP_lit0 + Below);
    } else {
      OS << uint8_t(dwarf::DW_OP_constu);
      encodeULEB128(Below, OS);
    }
    OS << uint8_t(dwarf::DW_OP_minus) << uint8_t(dwarf::DW_OP_deref);
    Comment = "r31 = [CFA-" + std::to_string(Below) + "] * 4";
  }
  OS << uint8_t(dwarf::DW_OP_lit2) << uint8_t(dwarf::DW_OP_shl);

  SmallString<24> Bytes;
  raw_svector_ostream BOS(Bytes);
  BOS << uint8_t(dwarf::DW_CFA_val_expression);
  encodeULEB128(31, BOS);
  encodeULEB128(Expr.size(), BOS);
  BOS << Expr;
  return MCCFIInstruction::createEscape(L, Bytes, SMLoc(), Comment);
}

// At entry, CFA = sp and the return address is r31 * 4 (docs/abi.md section
// 8.1 and 2.4).
static MCAsmInfo *createMIPSXMCAsmInfo(const MCRegisterInfo &MRI,
                                       const Triple &TT,
                                       const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new MIPSXMCAsmInfo(TT, Options);
  MAI->addInitialFrameState(MCCFIInstruction::cfiDefCfa(
      nullptr, MRI.getDwarfRegNum(MIPSX::R29, true), 0));
  MAI->addInitialFrameState(MIPSX::createReturnAddressCFI(nullptr, {}));
  return MAI;
}

static MCSubtargetInfo *
createMIPSXMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  if (CPU.empty())
    CPU = "es3890";
  return createMIPSXMCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
}

static MCInstPrinter *createMIPSXMCInstPrinter(const Triple & /*T*/,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new MIPSXInstPrinter(MAI, MII, MRI);
  return nullptr;
}

namespace {
// Sets e_flags from the subtarget (docs/abi.md section 11.2).
class MIPSXELFTargetStreamer : public MCTargetStreamer {
public:
  MIPSXELFTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI)
      : MCTargetStreamer(S) {
    ELFObjectWriter &W = static_cast<MCELFStreamer &>(S).getWriter();
    unsigned EFlags = W.getELFHeaderEFlags();
    if (STI.hasFeature(MIPSX::FeatureES3890))
      EFlags |= ELF::EF_MIPSX_ES3890;
    W.setELFHeaderEFlags(EFlags);
  }
};
} // end anonymous namespace

static MCTargetStreamer *
createMIPSXObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  return new MIPSXELFTargetStreamer(S, STI);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMIPSXTargetMC() {
  Target &T = getTheMIPSXTarget();
  RegisterMCAsmInfoFn X(T, createMIPSXMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createMIPSXMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createMIPSXMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createMIPSXMCSubtargetInfo);
  TargetRegistry::RegisterMCCodeEmitter(T, createMIPSXMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createMIPSXAsmBackend);
  TargetRegistry::RegisterMCInstPrinter(T, createMIPSXMCInstPrinter);
  TargetRegistry::RegisterObjectTargetStreamer(T,
                                               createMIPSXObjectTargetStreamer);
}
