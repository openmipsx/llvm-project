//===-- MIPSXMCAsmInfo.h - MIPS-X asm properties ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCASMINFO_H
#define LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCASMINFO_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCAsmInfoELF.h"
#include <cstdint>

namespace llvm {
class Triple;

class MIPSXMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit MIPSXMCAsmInfo(const Triple &TheTriple,
                          const MCTargetOptions &Options);
  void printSpecifierExpr(raw_ostream &OS,
                          const MCSpecifierExpr &Expr) const override;
};

namespace MIPSX {
// Relocation operators of docs/abi.md section 10.6, carried as the specifier
// of an MCSpecifierExpr.
using Specifier = uint16_t;
enum {
  S_None,
  S_HI,    // %hi(x): (x) >> 15
  S_LO,    // %lo(x): (x) & 0x7fff
  S_GPREL, // %gprel(x): x - _gp
  S_CALL,  // %call(x): offset from a linker-selected code base
  S_W,     // %w(x): (x) >> 2, a word address
};

/// Return the specifier for an operator name ("hi", "lo", ...), or S_None.
Specifier parseSpecifierName(StringRef Name);
/// Return the operator name of a specifier.
StringRef getSpecifierName(Specifier S);
} // namespace MIPSX

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCASMINFO_H
