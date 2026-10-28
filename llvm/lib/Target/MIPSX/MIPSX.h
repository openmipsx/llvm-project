//===-- MIPSX.h - Top-level interface for the MIPS-X target -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MIPSX_MIPSX_H
#define LLVM_LIB_TARGET_MIPSX_MIPSX_H

namespace llvm {
class FunctionPass;
class MIPSXTargetMachine;
class PassRegistry;

FunctionPass *createMIPSXISelDag(MIPSXTargetMachine &TM);
FunctionPass *createMIPSXHazardPaddingPass();

void initializeMIPSXAsmPrinterPass(PassRegistry &);
void initializeMIPSXDAGToDAGISelLegacyPass(PassRegistry &);
void initializeMIPSXHazardPaddingPass(PassRegistry &);
} // namespace llvm

#endif // LLVM_LIB_TARGET_MIPSX_MIPSX_H
