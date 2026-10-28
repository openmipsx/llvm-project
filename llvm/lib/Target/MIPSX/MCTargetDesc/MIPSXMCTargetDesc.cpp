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
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
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
  RegisterMCAsmInfo<MIPSXMCAsmInfo> X(T);
  TargetRegistry::RegisterMCInstrInfo(T, createMIPSXMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createMIPSXMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createMIPSXMCSubtargetInfo);
  TargetRegistry::RegisterMCCodeEmitter(T, createMIPSXMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createMIPSXAsmBackend);
  TargetRegistry::RegisterMCInstPrinter(T, createMIPSXMCInstPrinter);
  TargetRegistry::RegisterObjectTargetStreamer(T,
                                               createMIPSXObjectTargetStreamer);
}
