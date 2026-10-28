//===-- MIPSXTargetObjectFile.h - MIPS-X object file info -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSXTARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_MIPSX_MIPSXTARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

// Small data (docs/abi.md section 6.3): variables of at most
// -mipsx-ssection-threshold bytes go in .sdata/.sbss and are reached
// through gp in one instruction.
class MIPSXTargetObjectFile : public TargetLoweringObjectFileELF {
  MCSection *SmallDataSection = nullptr;
  MCSection *SmallBSSSection = nullptr;

public:
  void Initialize(MCContext &Ctx, const TargetMachine &TM) override;

  // Whether GO is (or, for a declaration, is assumed to be) in small data.
  bool isGlobalInSmallSection(const GlobalObject *GO,
                              const TargetMachine &TM) const;

  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override;

private:
  bool isGlobalInSmallSection(const GlobalObject *GO, const TargetMachine &TM,
                              SectionKind Kind) const;
  bool isGlobalInSmallSectionImpl(const GlobalObject *GO) const;
};

} // end namespace llvm

#endif
