//===-- MIPSXMCAsmInfo.cpp - MIPS-X asm properties ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXMCAsmInfo.h"
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
}
