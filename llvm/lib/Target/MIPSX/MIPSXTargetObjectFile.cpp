//===-- MIPSXTargetObjectFile.cpp - MIPS-X object file info ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MIPSXTargetObjectFile.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

// The same default as GCC's -G for MIPS. All objects of a program must use
// the same threshold, or at least agree on which external variables are
// small; a disagreement shows as an R_MIPSX_GPREL17 range error at link
// time, never as a wrong address.
static cl::opt<unsigned> SSThreshold(
    "mipsx-ssection-threshold", cl::Hidden, cl::init(8),
    cl::desc("MIPS-X: largest variable, in bytes, placed in .sdata/.sbss and "
             "reached through gp (0 disables small data)"));

static cl::opt<bool> ExternSData(
    "mipsx-extern-sdata", cl::Hidden, cl::init(true),
    cl::desc("MIPS-X: reach small variables defined in other objects "
             "through gp"));

void MIPSXTargetObjectFile::Initialize(MCContext &Ctx,
                                       const TargetMachine &TM) {
  TargetLoweringObjectFileELF::Initialize(Ctx, TM);
  SmallDataSection = getContext().getELFSection(
      ".sdata", ELF::SHT_PROGBITS, ELF::SHF_WRITE | ELF::SHF_ALLOC);
  SmallBSSSection = getContext().getELFSection(
      ".sbss", ELF::SHT_NOBITS, ELF::SHF_WRITE | ELF::SHF_ALLOC);
}

bool MIPSXTargetObjectFile::isGlobalInSmallSectionImpl(
    const GlobalObject *GO) const {
  const auto *GV = dyn_cast<GlobalVariable>(GO);
  if (!GV || GV->isThreadLocal())
    return false;

  // An explicit section decides by its name.
  if (GV->hasSection()) {
    StringRef Name = GV->getSection();
    return Name == ".sdata" || Name.starts_with(".sdata.") ||
           Name == ".sbss" || Name.starts_with(".sbss.");
  }

  if (SSThreshold == 0)
    return false;
  // An undefined weak variable has address 0, out of gp's reach; a common
  // one is placed by the linker (in .bss, not .sbss); a comdat one would
  // need its own group of small sections.
  if (GV->hasExternalWeakLinkage() || GV->hasCommonLinkage() ||
      GV->hasComdat())
    return false;
  if (GV->isDeclaration() && !ExternSData)
    return false;

  Type *Ty = GV->getValueType();
  if (!Ty->isSized())
    return false;
  uint64_t Size = GV->getDataLayout().getTypeAllocSize(Ty);
  // Zero-sized objects are not small data, as in GCC.
  return Size > 0 && Size <= SSThreshold;
}

bool MIPSXTargetObjectFile::isGlobalInSmallSection(
    const GlobalObject *GO, const TargetMachine &TM) const {
  // getKindForGlobal is only defined for definitions.
  if (GO->isDeclaration() || GO->hasAvailableExternallyLinkage())
    return isGlobalInSmallSectionImpl(GO);
  return isGlobalInSmallSection(GO, TM, getKindForGlobal(GO, TM));
}

bool MIPSXTargetObjectFile::isGlobalInSmallSection(const GlobalObject *GO,
                                                   const TargetMachine &TM,
                                                   SectionKind Kind) const {
  // Small constants go in .sdata too, so that gp reaches them.
  return isGlobalInSmallSectionImpl(GO) &&
         (Kind.isData() || Kind.isBSS() || Kind.isReadOnly());
}

MCSection *MIPSXTargetObjectFile::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  if (isGlobalInSmallSection(GO, TM, Kind)) {
    bool BSS = Kind.isBSS();
    if (!TM.getDataSections())
      return BSS ? SmallBSSSection : SmallDataSection;
    // -fdata-sections: one section per variable, for --gc-sections.
    SmallString<64> Name(BSS ? ".sbss." : ".sdata.");
    Name += TM.getSymbol(GO)->getName();
    return getContext().getELFSection(Name, BSS ? ELF::SHT_NOBITS
                                                : ELF::SHT_PROGBITS,
                                      ELF::SHF_WRITE | ELF::SHF_ALLOC);
  }
  return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);
}
