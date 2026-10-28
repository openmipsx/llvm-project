//===-- MIPSXAsmBackend.cpp - MIPS-X assembler backend --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Fixup resolution and nop padding. Units and formulas are those of the
// MIPS-X SDK docs/abi.md section 12: symbol values and addends are bytes,
// branch displacements and %w values are words.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXFixupKinds.h"
#include "MCTargetDesc/MIPSXMCAsmInfo.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

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

  std::optional<MCFixupKind> getFixupKind(StringRef Name) const override;
  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;
};
} // namespace

// Relocation names for the .reloc directive.
std::optional<MCFixupKind> MIPSXAsmBackend::getFixupKind(StringRef Name) const {
  unsigned Type = llvm::StringSwitch<unsigned>(Name)
#define ELF_RELOC(X, Y) .Case(#X, Y)
#include "llvm/BinaryFormat/ELFRelocs/MIPSX.def"
#undef ELF_RELOC
                      .Default(-1u);
  if (Type != -1u)
    return static_cast<MCFixupKind>(FirstLiteralRelocationKind + Type);
  return std::nullopt;
}

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

  if (mc::isRelocation(Kind))
    return {};
  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < MIPSX::NumTargetFixupKinds &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

// Compute the field value of a resolved fixup, or report an error. Value is
// S + A, or S + A - P for PC-relative fixups.
static std::optional<uint64_t> adjustFixupValue(const MCFixup &Fixup,
                                                const MCValue &Target,
                                                uint64_t Value,
                                                MCContext &Ctx) {
  int64_t V = static_cast<int32_t>(Value);
  auto Fail = [&](const Twine &Msg) -> std::optional<uint64_t> {
    Ctx.reportError(Fixup.getLoc(), Msg);
    return std::nullopt;
  };
  switch (static_cast<unsigned>(Fixup.getKind())) {
  case FK_Data_1:
  case FK_Data_2:
    return Value;
  case FK_Data_4:
    if (Target.getSpecifier() == MIPSX::S_W) {
      if (Value & 3)
        return Fail("%w value is not a multiple of 4");
      return (Value & 0xffffffff) >> 2;
    }
    return Value;
  case MIPSX::fixup_mipsx_pc16: {
    if (V & 3)
      return Fail("branch target is not a multiple of 4 bytes away");
    int64_t Disp = (V >> 2) - 2;
    if (!isInt<16>(Disp))
      return Fail("branch target out of range (displacement " + Twine(Disp) +
                  " words, limit -32768 to 32767)");
    return Disp & 0xffff;
  }
  case MIPSX::fixup_mipsx_hi17:
    return (V >> 15) & 0x1ffff;
  case MIPSX::fixup_mipsx_lo15:
    return V & 0x7fff;
  case MIPSX::fixup_mipsx_abs17:
    if (!isInt<17>(V))
      return Fail("value " + Twine(V) +
                  " out of range of a 17-bit signed immediate");
    return V & 0x1ffff;
  case MIPSX::fixup_mipsx_word17:
    if (V & 3)
      return Fail("%w value is not a multiple of 4");
    if (!isInt<17>(V >> 2))
      return Fail("%w value " + Twine(V >> 2) +
                  " out of range of a 17-bit signed immediate");
    return (V >> 2) & 0x1ffff;
  case MIPSX::fixup_mipsx_32w:
    if (Value & 3)
      return Fail("%w value is not a multiple of 4");
    return (Value & 0xffffffff) >> 2;
  case MIPSX::fixup_mipsx_call17:
  case MIPSX::fixup_mipsx_gprel17:
    llvm_unreachable("always relocated");
  }
  return Fail("unsupported fixup");
}

void MIPSXAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                 const MCValue &Target, uint8_t *Data,
                                 uint64_t Value, bool IsResolved) {
  // The code base and _gp are known only to the linker.
  MCFixupKind Kind = Fixup.getKind();
  if (Kind == MIPSX::fixup_mipsx_call17 || Kind == MIPSX::fixup_mipsx_gprel17)
    IsResolved = false;

  maybeAddReloc(F, Fixup, Target, Value, IsResolved);
  if (!IsResolved || mc::isRelocation(Kind))
    return; // RELA: the field stays zero and the addend is in the relocation.

  std::optional<uint64_t> Field =
      adjustFixupValue(Fixup, Target, Value, getContext());
  if (!Field)
    return;

  // Data fixups cover their own size; instruction fixups are in the low bits
  // of a big-endian word.
  unsigned NumBytes;
  uint64_t Mask;
  switch (static_cast<unsigned>(Kind)) {
  case FK_Data_1:
    NumBytes = 1;
    Mask = 0xff;
    break;
  case FK_Data_2:
    NumBytes = 2;
    Mask = 0xffff;
    break;
  case FK_Data_4:
  case MIPSX::fixup_mipsx_32w:
    NumBytes = 4;
    Mask = 0xffffffff;
    break;
  case MIPSX::fixup_mipsx_pc16:
    NumBytes = 4;
    Mask = 0xffff;
    break;
  default:
    NumBytes = 4;
    Mask = 0x1ffff;
    break;
  }

  uint64_t Cur = 0;
  for (unsigned I = 0; I != NumBytes; ++I)
    Cur = (Cur << 8) | Data[I];
  Cur = (Cur & ~Mask) | (*Field & Mask);
  for (unsigned I = 0; I != NumBytes; ++I)
    Data[I] = static_cast<uint8_t>(Cur >> (8 * (NumBytes - 1 - I)));
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
