//===- MIPSX.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MIPS-X is the Stanford MIPS-X processor as used in the ESS ES3890. It is a
// 32-bit big-endian RISC with byte-addressed data and word-addressed code:
// branch displacements, jump targets and return addresses count 32-bit
// words. As on AVR, ELF symbol values and addends are byte addresses and the
// relocations convert to words.
//
// Relocations are those of the MIPS-X SDK ABI document (docs/abi.md section
// 12). R_MIPSX_CALL17 makes jspci reach its target through one of the code
// base registers: the linker picks r0 (base 0), r23 (__mipsx_cb0 / 4) or r24
// (__mipsx_cb1 / 4) and rewrites the instruction's Src1 field.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "SymbolTable.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class MIPSX final : public TargetInfo {
public:
  MIPSX(Ctx &);
  uint32_t calcEFlags() const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;

private:
  std::optional<uint64_t> getSymbolVA(StringRef name) const;
};
} // namespace

MIPSX::MIPSX(Ctx &ctx) : TargetInfo(ctx) {
  symbolicRel = R_MIPSX_32;
  // hsc (halt).
  trapInstr = {0xcf, 0xc0, 0x00, 0x00};
  defaultImageBase = 0;
}

uint32_t MIPSX::calcEFlags() const {
  // The output uses every extension that any input uses.
  uint32_t flags = 0;
  for (ELFFileBase *f : ctx.objectFiles) {
    uint32_t eflags = f->getObj<ELF32BE>().getHeader().e_flags;
    if (eflags & ~uint32_t(EF_MIPSX_ES3890 | EF_MIPSX_VSHIFT))
      Err(ctx) << f << ": unknown e_flags 0x" << utohexstr(eflags);
    flags |= eflags;
  }
  return flags;
}

RelExpr MIPSX::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  switch (type) {
  case R_MIPSX_NONE:
    return R_NONE;
  case R_MIPSX_32:
  case R_MIPSX_16:
  case R_MIPSX_8:
  case R_MIPSX_32W:
  case R_MIPSX_CALL17:
  case R_MIPSX_HI17:
  case R_MIPSX_LO15:
  case R_MIPSX_ABS17:
  case R_MIPSX_WORD17:
  case R_MIPSX_GPREL17:
    return R_ABS;
  case R_MIPSX_32_PCREL:
  case R_MIPSX_PC16:
    return R_PC;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unknown relocation (" << type.v
             << ") against symbol " << &s;
    return R_NONE;
  }
}

std::optional<uint64_t> MIPSX::getSymbolVA(StringRef name) const {
  Symbol *s = ctx.symtab->find(name);
  if (!s || !s->isDefined())
    return std::nullopt;
  return s->getVA(ctx);
}

// Write a 17-bit immediate or offset field (bits 16-0).
static void writeImm17(uint8_t *loc, uint64_t v) {
  write32be(loc, (read32be(loc) & ~0x1ffffu) | (v & 0x1ffff));
}

void MIPSX::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_MIPSX_32:
  case R_MIPSX_32_PCREL:
    checkIntUInt(ctx, loc, val, 32, rel);
    write32be(loc, val);
    break;
  case R_MIPSX_16:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16be(loc, val);
    break;
  case R_MIPSX_8:
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_MIPSX_32W:
    checkAlignment(ctx, loc, val, 4, rel);
    write32be(loc, uint32_t(val) >> 2);
    break;
  case R_MIPSX_PC16: {
    // Target word = branch word + 2 + displacement.
    checkAlignment(ctx, loc, val, 4, rel);
    int64_t disp = (int64_t(val) >> 2) - 2;
    checkInt(ctx, loc, disp, 16, rel);
    write32be(loc, (read32be(loc) & ~0xffffu) | (disp & 0xffff));
    break;
  }
  case R_MIPSX_HI17:
    writeImm17(loc, int32_t(val) >> 15);
    break;
  case R_MIPSX_LO15:
    writeImm17(loc, val & 0x7fff);
    break;
  case R_MIPSX_ABS17:
    checkInt(ctx, loc, int32_t(val), 17, rel);
    writeImm17(loc, val);
    break;
  case R_MIPSX_WORD17: {
    checkAlignment(ctx, loc, val, 4, rel);
    int64_t w = int32_t(val) >> 2;
    checkInt(ctx, loc, w, 17, rel);
    writeImm17(loc, w);
    break;
  }
  case R_MIPSX_GPREL17: {
    std::optional<uint64_t> gp = getSymbolVA("_gp");
    if (!gp) {
      Err(ctx) << getErrorLoc(ctx, loc) << rel.type
               << " needs _gp, which is not defined";
      return;
    }
    int64_t off = int32_t(val - *gp);
    checkInt(ctx, loc, off, 17, rel);
    writeImm17(loc, off);
    break;
  }
  case R_MIPSX_CALL17: {
    // Try r0, cb0 and cb1 in turn (docs/abi.md section 5.2).
    checkAlignment(ctx, loc, val, 4, rel);
    int64_t target = int64_t(uint32_t(val) >> 2);
    struct Base {
      unsigned reg;
      std::optional<uint64_t> va;
    } bases[] = {{0, 0},
                 {23, getSymbolVA("__mipsx_cb0")},
                 {24, getSymbolVA("__mipsx_cb1")}};
    for (const Base &b : bases) {
      if (!b.va)
        continue;
      int64_t off = target - int64_t(uint32_t(*b.va) >> 2);
      if (!isInt<17>(off))
        continue;
      uint32_t insn = read32be(loc);
      insn = (insn & ~(0x1fu << 22)) | (b.reg << 22);
      insn = (insn & ~0x1ffffu) | (off & 0x1ffff);
      write32be(loc, insn);
      return;
    }
    Err(ctx) << getErrorLoc(ctx, loc) << rel.type << " target 0x"
             << utohexstr(uint32_t(val)) << " (" << rel.sym
             << ") is out of reach of r0, __mipsx_cb0 and __mipsx_cb1"
             << "; place the code nearer a code base or use -mlong-calls";
    break;
  }
  case R_MIPSX_NONE:
    break;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
  }
}

void elf::setMIPSXTargetInfo(Ctx &ctx) { ctx.target.reset(new MIPSX(ctx)); }
