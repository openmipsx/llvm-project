//===-- MIPSXFixupKinds.h - MIPS-X specific fixup entries -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// One fixup kind per relocation of MIPS-X SDK docs/abi.md section 12. Data
// relocations (R_MIPSX_32, _16, _8, _32_PCREL) use the generic FK_Data kinds.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXFIXUPKINDS_H
#define LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace MIPSX {
enum Fixups {
  // disp16 = ((S + A - P) >> 2) - 2, branch displacement in words.
  fixup_mipsx_pc16 = FirstTargetFixupKind,
  // imm17 and Src1: offset from a linker-selected code base (%call).
  fixup_mipsx_call17,
  // imm17 = (S + A) >> 15 (%hi).
  fixup_mipsx_hi17,
  // imm17 = (S + A) & 0x7fff (%lo).
  fixup_mipsx_lo15,
  // imm17 = S + A, plain symbol in an immediate or offset.
  fixup_mipsx_abs17,
  // imm17 = (S + A) >> 2 (%w in an immediate).
  fixup_mipsx_word17,
  // imm17 = S + A - _gp (%gprel).
  fixup_mipsx_gprel17,
  // 32-bit data word = (S + A) >> 2 (.word %w(sym)).
  fixup_mipsx_32w,

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // namespace MIPSX
} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXFIXUPKINDS_H
