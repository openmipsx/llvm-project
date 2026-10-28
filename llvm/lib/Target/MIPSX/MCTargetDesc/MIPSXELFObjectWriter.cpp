//===-- MIPSXELFObjectWriter.cpp - MIPS-X ELF writer ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ELF objects for EM_MIPS_X, with RELA relocations (MIPS-X SDK docs/abi.md
// sections 11 and 12).
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXFixupKinds.h"
#include "MCTargetDesc/MIPSXMCAsmInfo.h"
#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

namespace {

class MIPSXELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit MIPSXELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit_=*/false, OSABI, ELF::EM_MIPS_X,
                                /*HasRelocationAddend_=*/true) {}

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override;
};

} // end anonymous namespace

unsigned MIPSXELFObjectWriter::getRelocType(const MCFixup &Fixup,
                                            const MCValue &Target,
                                            bool IsPCRel) const {
  bool IsW = Target.getSpecifier() == MIPSX::S_W;
  switch (static_cast<unsigned>(Fixup.getKind())) {
  case FK_NONE:
    return ELF::R_MIPSX_NONE;
  case FK_Data_4:
    if (IsW) {
      if (IsPCRel)
        reportError(Fixup.getLoc(), "%w cannot be PC-relative");
      return ELF::R_MIPSX_32W;
    }
    return IsPCRel ? ELF::R_MIPSX_32_PCREL : ELF::R_MIPSX_32;
  case FK_Data_2:
  case FK_Data_1:
    if (IsPCRel || IsW) {
      reportError(Fixup.getLoc(),
                  "unsupported relocation in 8- or 16-bit data");
      return ELF::R_MIPSX_NONE;
    }
    return Fixup.getKind() == FK_Data_2 ? ELF::R_MIPSX_16 : ELF::R_MIPSX_8;
  case MIPSX::fixup_mipsx_32w:
    return ELF::R_MIPSX_32W;
  case MIPSX::fixup_mipsx_pc16:
    return ELF::R_MIPSX_PC16;
  case MIPSX::fixup_mipsx_call17:
    return ELF::R_MIPSX_CALL17;
  case MIPSX::fixup_mipsx_hi17:
    return ELF::R_MIPSX_HI17;
  case MIPSX::fixup_mipsx_lo15:
    return ELF::R_MIPSX_LO15;
  case MIPSX::fixup_mipsx_abs17:
    return ELF::R_MIPSX_ABS17;
  case MIPSX::fixup_mipsx_word17:
    return ELF::R_MIPSX_WORD17;
  case MIPSX::fixup_mipsx_gprel17:
    return ELF::R_MIPSX_GPREL17;
  }
  reportError(Fixup.getLoc(), "unsupported relocation type");
  return ELF::R_MIPSX_NONE;
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createMIPSXELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<MIPSXELFObjectWriter>(OSABI);
}
