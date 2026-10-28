//===-- MIPSXMCAsmInfo.cpp - MIPS-X asm properties ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXMCAsmInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void MIPSXMCAsmInfo::anchor() {}

MIPSXMCAsmInfo::MIPSXMCAsmInfo(const Triple & /*TheTriple*/,
                               const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  IsLittleEndian = false;
  CodePointerSize = 4;
  CalleeSaveStackSlotSize = 4;
  InternalSymbolPrefix = ".L";
  ExceptionsType = ExceptionHandling::DwarfCFI;
  SupportsDebugInformation = true;
  MinInstAlignment = 4;

  // ';' starts a comment, as in the MIPS-X manual. '#' marks immediates, so
  // it cannot be a comment character, and ';' is not free as a separator.
  CommentString = ";";
  SeparatorString = "%%";

  // .align takes a power of two, as on MIPS.
  AlignmentIsInBytes = false;

  // The manual's data directives, parsed by MIPSXAsmParser.
  Data16bitsDirective = "\t.half\t";
  Data32bitsDirective = "\t.word\t";
}

MIPSX::Specifier MIPSX::parseSpecifierName(StringRef Name) {
  return StringSwitch<MIPSX::Specifier>(Name)
      .Case("hi", S_HI)
      .Case("lo", S_LO)
      .Case("gprel", S_GPREL)
      .Case("call", S_CALL)
      .Case("w", S_W)
      .Default(S_None);
}

StringRef MIPSX::getSpecifierName(Specifier S) {
  switch (S) {
  case S_HI:
    return "hi";
  case S_LO:
    return "lo";
  case S_GPREL:
    return "gprel";
  case S_CALL:
    return "call";
  case S_W:
    return "w";
  }
  llvm_unreachable("Invalid MIPS-X specifier");
}

void MIPSXMCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                        const MCSpecifierExpr &Expr) const {
  OS << '%' << MIPSX::getSpecifierName(Expr.getSpecifier()) << '(';
  printExpr(OS, *Expr.getSubExpr());
  OS << ')';
}
