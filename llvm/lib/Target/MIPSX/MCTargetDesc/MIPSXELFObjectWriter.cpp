//===-- MIPSXELFObjectWriter.cpp - MIPS-X ELF writer ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ELF objects for EM_MIPS_X, with RELA relocations. Relocation types are not
// defined yet, so any relocation is reported as an error.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MIPSXMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"

using namespace llvm;

namespace {

class MIPSXELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit MIPSXELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit_=*/false, OSABI, ELF::EM_MIPS_X,
                                /*HasRelocationAddend_=*/true) {}

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &,
                        bool) const override {
    reportError(Fixup.getLoc(), "MIPS-X relocations are not supported yet");
    return 0;
  }
};

} // end anonymous namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createMIPSXELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<MIPSXELFObjectWriter>(OSABI);
}
