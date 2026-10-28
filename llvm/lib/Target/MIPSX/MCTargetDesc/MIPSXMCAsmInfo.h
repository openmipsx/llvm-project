//===-- MIPSXMCAsmInfo.h - MIPS-X asm properties ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCASMINFO_H
#define LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class MIPSXMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit MIPSXMCAsmInfo(const Triple &TheTriple,
                          const MCTargetOptions &Options);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MCTARGETDESC_MIPSXMCASMINFO_H
