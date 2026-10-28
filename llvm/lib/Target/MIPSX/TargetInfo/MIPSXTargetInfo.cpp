//===-- MIPSXTargetInfo.cpp - MIPS-X target implementation ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/MIPSXTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &llvm::getTheMIPSXTarget() {
  static Target TheMIPSXTarget;
  return TheMIPSXTarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMIPSXTargetInfo() {
  RegisterTarget<Triple::mipsx> X(getTheMIPSXTarget(), "mipsx",
                                  "MIPS-X (ES3890)", "MIPSX");
}
