//===-- MIPSXAsmBackend.cpp - MIPS-X assembler backend --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Fixup descriptions and nop padding. Resolving fixups and emitting
// relocations is not implemented yet: the backend is enough for
// "llvm-mc -show-encoding" and for objects without fixups.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXFixupKinds.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class MIPSXAsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  MIPSXAsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::big), OSABI(OSABI) {}

  void applyFixup(const MCFragment &, const MCFixup &, const MCValue &Target,
                  uint8_t *Data, uint64_t Value, bool IsResolved) override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createMIPSXELFObjectWriter(OSABI);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;
};
} // namespace

MCFixupKindInfo MIPSXAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  // Must be in the order of the fixup kinds in MIPSXFixupKinds.h. For a
  // big-endian target the bit offset counts from the most significant bit of
  // the word, so a field in bits n-1..0 has offset 32 - n. CALL17 also writes
  // the Src1 field (bits 26-22), which is not described here.
  static const MCFixupKindInfo Infos[MIPSX::NumTargetFixupKinds] = {
      // name                 offset bits flags
      {"fixup_mipsx_pc16", 16, 16, 0},
      {"fixup_mipsx_call17", 15, 17, 0},
      {"fixup_mipsx_hi17", 15, 17, 0},
      {"fixup_mipsx_lo15", 15, 17, 0},
      {"fixup_mipsx_abs17", 15, 17, 0},
      {"fixup_mipsx_word17", 15, 17, 0},
      {"fixup_mipsx_gprel17", 15, 17, 0},
      {"fixup_mipsx_32w", 0, 32, 0},
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < MIPSX::NumTargetFixupKinds &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

void MIPSXAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                 const MCValue &Target, uint8_t *Data,
                                 uint64_t Value, bool IsResolved) {
  getContext().reportError(Fixup.getLoc(),
                           "MIPS-X fixups are not supported yet");
}

bool MIPSXAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                   const MCSubtargetInfo *STI) const {
  if (Count % 4 != 0)
    return false;
  // nop is add r0,r0,r0.
  for (uint64_t I = 0; I != Count; I += 4)
    support::endian::write<uint32_t>(OS, 0x60000019, llvm::endianness::big);
  return true;
}

MCAsmBackend *llvm::createMIPSXAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo & /*MRI*/,
                                          const MCTargetOptions & /*Options*/) {
  const Triple &TT = STI.getTargetTriple();
  return new MIPSXAsmBackend(MCELFObjectTargetWriter::getOSABI(TT.getOS()));
}
